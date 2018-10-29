// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/dice_response_handler.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/signin/account_management_screen_helper.h"
#else
#include "chrome/browser/ui/browser_commands.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/chromeos_switches.h"
#endif

namespace signin {

namespace {

const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";

// Key for RequestDestructionObserverUserData.
const void* const kRequestDestructionObserverUserDataKey =
    &kRequestDestructionObserverUserDataKey;

// TODO(droger): Remove this delay when the Dice implementation is finished on
// the server side.
int g_dice_account_reconcilor_blocked_delay_ms = 1000;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

const char kGoogleSignoutResponseHeader[] = "Google-Accounts-SignOut";

// Refcounted wrapper to allow creating and deleting a AccountReconcilor::Lock
// from the IO thread.
class AccountReconcilorLockWrapper
    : public base::RefCountedThreadSafe<AccountReconcilorLockWrapper> {
 public:
  AccountReconcilorLockWrapper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // Do nothing on the IO thread. The real work is done in CreateLockOnUI().
  }

  // Creates the account reconcilor lock on the UI thread. The lock will be
  // deleted on the UI thread when this wrapper is deleted.
  void CreateLockOnUI(const content::ResourceRequestInfo::WebContentsGetter&
                          web_contents_getter) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::WebContents* web_contents = web_contents_getter.Run();
    if (!web_contents)
      return;
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    AccountReconcilor* account_reconcilor =
        AccountReconcilorFactory::GetForProfile(profile);
    account_reconcilor_lock_.reset(
        new AccountReconcilor::Lock(account_reconcilor));
  }

 private:
  friend class base::RefCountedThreadSafe<AccountReconcilorLockWrapper>;
  ~AccountReconcilorLockWrapper() {}

  // The account reconcilor lock is created and deleted on UI thread.
  std::unique_ptr<AccountReconcilor::Lock,
                  content::BrowserThread::DeleteOnUIThread>
      account_reconcilor_lock_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorLockWrapper);
};

void DestroyLockWrapperAfterDelay(
    scoped_refptr<AccountReconcilorLockWrapper> lock_wrapper) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          base::DoNothing::Once<scoped_refptr<AccountReconcilorLockWrapper>>(),
          std::move(lock_wrapper)),
      base::TimeDelta::FromMilliseconds(
          g_dice_account_reconcilor_blocked_delay_ms));
}

// Returns true if the account reconcilor needs be be blocked while a Gaia
// sign-in request is in progress.
//
// The account reconcilor must be blocked on all request that may change the
// Gaia authentication cookies. This includes:
// * Main frame  requests.
// * XHR requests having Gaia URL as referrer.
bool ShouldBlockReconcilorForRequest(ChromeRequestAdapter* request) {
  content::ResourceType resource_type = request->GetResourceType();

  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME)
    return true;

  return (resource_type == content::RESOURCE_TYPE_XHR) &&
         gaia::IsGaiaSignonRealm(request->GetReferrerOrigin());
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class RequestDestructionObserverUserData : public base::SupportsUserData::Data {
 public:
  explicit RequestDestructionObserverUserData(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  ~RequestDestructionObserverUserData() override { std::move(closure_).Run(); }

 private:
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(RequestDestructionObserverUserData);
};

// Processes the mirror response header on the UI thread. Currently depending
// on the value of |header_value|, it either shows the profile avatar menu, or
// opens an incognito window/tab.
void ProcessMirrorHeaderUIThread(
    ManageAccountsParams manage_accounts_params,
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GAIAServiceType service_type = manage_accounts_params.service_type;
  DCHECK_NE(GAIA_SERVICE_TYPE_NONE, service_type);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile))
      << "Gaia should not send the X-Chrome-Manage-Accounts header "
      << "when Mirror is disabled.";
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  account_reconcilor->OnReceivedManageAccountsResponse(service_type);
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    BrowserWindow::AvatarBubbleMode bubble_mode;
    switch (service_type) {
      case GAIA_SERVICE_TYPE_INCOGNITO:
        chrome::NewIncognitoWindow(profile);
        return;
      case GAIA_SERVICE_TYPE_ADDSESSION:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_ADD_ACCOUNT;
        break;
      case GAIA_SERVICE_TYPE_REAUTH:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH;
        break;
      default:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT;
    }
    signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
        account_reconcilor->GetState());

#if defined(OS_CHROMEOS)
    if (chromeos::switches::IsAccountManagerEnabled()) {
      // Chrome OS Account Manager is available. The only allowed operations
      // are:
      //
      // - Going Incognito (already handled in above switch-case).
      // - Displaying the Account Manager for managing accounts.
      chrome::SettingsWindowManager::GetInstance()->ShowChromePageForProfile(
          profile, GURL("chrome://settings/accountManager"));
      return;
    }

    // TODO(sinhak): Remove this when Chrome OS Account Manager is released.
    // Chrome OS does not have an account picker right now. To fix
    // https://crbug.com/807568, this is a no-op here. This is OK because in
    // the limited cases where Mirror is available on Chrome OS, 1:1 account
    // consistency is enforced and adding/removing accounts is not allowed,
    // GAIA_SERVICE_TYPE_INCOGNITO may be allowed though.
    return;
#endif

    browser->window()->ShowAvatarBubbleFromAvatarButton(
        bubble_mode, manage_accounts_params,
        signin_metrics::AccessPoint::ACCESS_POINT_CONTENT_AREA, false);
  }
#else   // defined(OS_ANDROID)
  if (service_type == signin::GAIA_SERVICE_TYPE_INCOGNITO) {
    GURL url(manage_accounts_params.continue_url.empty()
                 ? chrome::kChromeUINativeNewTabURL
                 : manage_accounts_params.continue_url);
    web_contents->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::OFF_THE_RECORD,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  } else {
    signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
        account_reconcilor->GetState());
    AccountManagementScreenHelper::OpenAccountManagementScreen(profile,
                                                               service_type);
  }
#endif  // !defined(OS_ANDROID)
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

// Creates a DiceTurnOnSyncHelper.
void CreateDiceTurnOnSyncHelper(Profile* profile,
                                signin_metrics::AccessPoint access_point,
                                signin_metrics::PromoAction promo_action,
                                signin_metrics::Reason reason,
                                content::WebContents* web_contents,
                                const std::string& account_id) {
  DCHECK(profile);
  Browser* browser = web_contents
                         ? chrome::FindBrowserWithWebContents(web_contents)
                         : chrome::FindBrowserWithProfile(profile);
  // DiceTurnSyncOnHelper is suicidal (it will kill itself once it finishes
  // enabling sync).
  new DiceTurnSyncOnHelper(
      profile, browser, access_point, promo_action, reason, account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
}

// Shows UI for signin errors.
void ShowDiceSigninError(Profile* profile,
                         content::WebContents* web_contents,
                         const std::string& error_message,
                         const std::string& email) {
  DCHECK(profile);
  Browser* browser = web_contents
                         ? chrome::FindBrowserWithWebContents(web_contents)
                         : chrome::FindBrowserWithProfile(profile);
  LoginUIServiceFactory::GetForProfile(profile)->DisplayLoginResult(
      browser, base::UTF8ToUTF16(error_message), base::UTF8ToUTF16(email));
}

void ProcessDiceHeaderUIThread(
    const DiceResponseParams& dice_params,
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(!profile->IsOffTheRecord());

  AccountConsistencyMethod account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  if (account_consistency == AccountConsistencyMethod::kMirror ||
      account_consistency == AccountConsistencyMethod::kDisabled) {
    // Ignore Dice response headers if Dice is not enabled at all.
    return;
  }

  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;
  signin_metrics::PromoAction promo_action =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  signin_metrics::Reason reason = signin_metrics::Reason::REASON_UNKNOWN_REASON;
  // This is the URL that the browser specified to redirect to after the user
  // signs in. Not to be confused with the redirect header from GAIA response.
  GURL redirect_after_signin_url = GURL::EmptyGURL();

  bool is_sync_signin_tab = false;
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents);
  if (tab_helper) {
    is_sync_signin_tab = true;
    access_point = tab_helper->signin_access_point();
    promo_action = tab_helper->signin_promo_action();
    reason = tab_helper->signin_reason();
    redirect_after_signin_url = tab_helper->redirect_url();
  }

  DiceResponseHandler* dice_response_handler =
      DiceResponseHandler::GetForProfile(profile);
  dice_response_handler->ProcessDiceHeader(
      dice_params,
      std::make_unique<ProcessDiceHeaderDelegateImpl>(
          web_contents, account_consistency,
          IdentityManagerFactory::GetForProfile(profile), is_sync_signin_tab,
          base::BindOnce(&CreateDiceTurnOnSyncHelper, base::Unretained(profile),
                         access_point, promo_action, reason),
          base::BindOnce(&ShowDiceSigninError, base::Unretained(profile)),
          redirect_after_signin_url));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Looks for the X-Chrome-Manage-Accounts response header, and if found,
// tries to show the avatar bubble in the browser identified by the
// child/route id. Must be called on IO thread.
void ProcessMirrorResponseHeaderIfExists(ResponseAdapter* response,
                                         bool is_off_the_record) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!response->IsMainFrame())
    return;

  if (!gaia::IsGaiaSignonRealm(response->GetOrigin()))
    return;

  const net::HttpResponseHeaders* response_headers = response->GetHeaders();
  if (!response_headers)
    return;

  std::string header_value;
  if (!response_headers->GetNormalizedHeader(kChromeManageAccountsHeader,
                                             &header_value)) {
    return;
  }

  if (is_off_the_record) {
    NOTREACHED() << "Gaia should not send the X-Chrome-Manage-Accounts header "
                 << "in incognito.";
    return;
  }

  ManageAccountsParams params = BuildManageAccountsParams(header_value);
  // If the request does not have a response header or if the header contains
  // garbage, then |service_type| is set to |GAIA_SERVICE_TYPE_NONE|.
  if (params.service_type == GAIA_SERVICE_TYPE_NONE)
    return;

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(ProcessMirrorHeaderUIThread, params,
                                          response->GetWebContentsGetter()));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProcessDiceResponseHeaderIfExists(ResponseAdapter* response,
                                       bool is_off_the_record) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (is_off_the_record)
    return;

  if (!gaia::IsGaiaSignonRealm(response->GetOrigin()))
    return;

  const net::HttpResponseHeaders* response_headers = response->GetHeaders();
  if (!response_headers)
    return;

  std::string header_value;
  DiceResponseParams params;
  if (response_headers->GetNormalizedHeader(kDiceResponseHeader,
                                            &header_value)) {
    params = BuildDiceSigninResponseParams(header_value);
    // The header must be removed for privacy reasons, so that renderers never
    // have access to the authorization code.
    response->RemoveHeader(kDiceResponseHeader);
  } else if (response_headers->GetNormalizedHeader(kGoogleSignoutResponseHeader,
                                                   &header_value)) {
    params = BuildDiceSignoutResponseParams(header_value);
  }

  // If the request does not have a response header or if the header contains
  // garbage, then |user_intention| is set to |NONE|.
  if (params.user_intention == DiceAction::NONE)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(ProcessDiceHeaderUIThread, base::Passed(std::move(params)),
                 response->GetWebContentsGetter()));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

ChromeRequestAdapter::ChromeRequestAdapter(net::URLRequest* request)
    : RequestAdapter(request) {}

ChromeRequestAdapter::~ChromeRequestAdapter() = default;

bool ChromeRequestAdapter::IsMainRequestContext(ProfileIOData* io_data) {
  return request_->context() == io_data->GetMainRequestContext();
}

content::ResourceRequestInfo::WebContentsGetter
ChromeRequestAdapter::GetWebContentsGetter() const {
  const auto* info = content::ResourceRequestInfo::ForRequest(request_);
  return info->GetWebContentsGetterForRequest();
}

content::ResourceType ChromeRequestAdapter::GetResourceType() const {
  const auto* info = content::ResourceRequestInfo::ForRequest(request_);
  return info->GetResourceType();
}

GURL ChromeRequestAdapter::GetReferrerOrigin() const {
  return GURL(request_->referrer()).GetOrigin();
}

void ChromeRequestAdapter::SetDestructionCallback(base::OnceClosure closure) {
  if (request_->GetUserData(kRequestDestructionObserverUserDataKey))
    return;

  request_->SetUserData(
      kRequestDestructionObserverUserDataKey,
      std::make_unique<RequestDestructionObserverUserData>(std::move(closure)));
}

ResponseAdapter::ResponseAdapter(const net::URLRequest* request)
    : request_(request) {}

ResponseAdapter::~ResponseAdapter() = default;

content::ResourceRequestInfo::WebContentsGetter
ResponseAdapter::GetWebContentsGetter() const {
  const auto* info = content::ResourceRequestInfo::ForRequest(request_);
  return info->GetWebContentsGetterForRequest();
}

bool ResponseAdapter::IsMainFrame() const {
  const auto* info = content::ResourceRequestInfo::ForRequest(request_);
  return info && (info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME);
}

GURL ResponseAdapter::GetOrigin() const {
  return request_->url().GetOrigin();
}

const net::HttpResponseHeaders* ResponseAdapter::GetHeaders() const {
  return request_->response_headers();
}

void ResponseAdapter::RemoveHeader(const std::string& name) {
  request_->response_headers()->RemoveHeader(name);
}

void SetDiceAccountReconcilorBlockDelayForTesting(int delay_ms) {
  g_dice_account_reconcilor_blocked_delay_ms = delay_ms;
}

void FixAccountConsistencyRequestHeader(ChromeRequestAdapter* request,
                                        const GURL& redirect_url,
                                        ProfileIOData* io_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (io_data->IsOffTheRecord())
    return;  // Account consistency is disabled in incognito.

  if (!request->IsMainRequestContext(io_data)) {
    // Account consistency requires the AccountReconcilor, which is only
    // attached to the main request context.
    // Note: InlineLoginUI uses an isolated request context and thus bypasses
    // the account consistency flow here. See http://crbug.com/428396
    return;
  }

  int profile_mode_mask = PROFILE_MODE_DEFAULT;
  if (io_data->incognito_availibility()->GetValue() ==
          IncognitoModePrefs::DISABLED ||
      IncognitoModePrefs::ArePlatformParentalControlsEnabled()) {
    profile_mode_mask |= PROFILE_MODE_INCOGNITO_DISABLED;
  }

  AccountConsistencyMethod account_consistency = io_data->account_consistency();

#if defined(OS_CHROMEOS)
  // Mirror account consistency required by profile.
  if (io_data->account_consistency_mirror_required()->GetValue()) {
    account_consistency = AccountConsistencyMethod::kMirror;
    // Can't add new accounts.
    profile_mode_mask |= PROFILE_MODE_ADD_ACCOUNT_DISABLED;
  }
#endif

  std::string account_id = io_data->google_services_account_id()->GetValue();

  // If new url is eligible to have the header, add it, otherwise remove it.

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Dice header:
  bool dice_header_added = AppendOrRemoveDiceRequestHeader(
      request, redirect_url, account_id, io_data->IsSyncEnabled(),
      io_data->SyncHasAuthError(), account_consistency,
      io_data->GetCookieSettings(), io_data->GetSigninScopedDeviceId());

  // Block the AccountReconcilor while the Dice requests are in flight. This
  // allows the DiceReponseHandler to process the response before the reconcilor
  // starts.
  if (dice_header_added && ShouldBlockReconcilorForRequest(request)) {
    auto lock_wrapper = base::MakeRefCounted<AccountReconcilorLockWrapper>();
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&AccountReconcilorLockWrapper::CreateLockOnUI,
                       lock_wrapper, request->GetWebContentsGetter()));

    // On destruction of the request |lock_wrapper| will be released.
    request->SetDestructionCallback(
        base::BindOnce(&DestroyLockWrapperAfterDelay, std::move(lock_wrapper)));
  }
#endif

  // Mirror header:
  AppendOrRemoveMirrorRequestHeader(
      request, redirect_url, account_id, account_consistency,
      io_data->GetCookieSettings(), profile_mode_mask);
}

void ProcessAccountConsistencyResponseHeaders(ResponseAdapter* response,
                                              const GURL& redirect_url,
                                              bool is_off_the_record) {
  if (redirect_url.is_empty()) {
    // This is not a redirect.

    // See if the response contains the X-Chrome-Manage-Accounts header. If so
    // show the profile avatar bubble so that user can complete signin/out
    // action the native UI.
    ProcessMirrorResponseHeaderIfExists(response, is_off_the_record);
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Process the Dice header: on sign-in, exchange the authorization code for a
  // refresh token, on sign-out just follow the sign-out URL.
  ProcessDiceResponseHeaderIfExists(response, is_off_the_record);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

}  // namespace signin
