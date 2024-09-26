// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/header_modification_delegate_impl.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/http/http_response_headers.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/android/view_android.h"
#else
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/dice_response_handler.h"
#include "chrome/browser/signin/dice_response_handler_factory.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#endif

namespace signin {

const void* const kManageAccountsHeaderReceivedUserDataKey =
    &kManageAccountsHeaderReceivedUserDataKey;

const char kChromeMirrorHeaderSource[] = "Chrome";

namespace {

// Key for RequestDestructionObserverUserData.
const void* const kRequestDestructionObserverUserDataKey =
    &kRequestDestructionObserverUserDataKey;

const char kGoogleRemoveLocalAccountResponseHeader[] =
    "Google-Accounts-RemoveLocalAccount";

const char kRemoveLocalAccountObfuscatedIDAttrName[] = "obfuscatedid";

// TODO(droger): Remove this delay when the Dice implementation is finished on
// the server side.
int g_dice_account_reconcilor_blocked_delay_ms = 1000;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

const char kGoogleSignoutResponseHeader[] = "Google-Accounts-SignOut";

// Refcounted wrapper that facilitates creating and deleting a
// AccountReconcilor::Lock.
class AccountReconcilorLockWrapper
    : public base::RefCountedThreadSafe<AccountReconcilorLockWrapper> {
 public:
  explicit AccountReconcilorLockWrapper(
      const content::WebContents::Getter& web_contents_getter) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::WebContents* web_contents = web_contents_getter.Run();
    if (!web_contents)
      return;
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    AccountReconcilor* account_reconcilor =
        AccountReconcilorFactory::GetForProfile(profile);
    account_reconcilor_lock_ =
        std::make_unique<AccountReconcilor::Lock>(account_reconcilor);
  }

  AccountReconcilorLockWrapper(const AccountReconcilorLockWrapper&) = delete;
  AccountReconcilorLockWrapper& operator=(const AccountReconcilorLockWrapper&) =
      delete;

  void DestroyAfterDelay() {
    // TODO(dcheng): Should ReleaseSoon() support this use case?
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, base::DoNothingWithBoundArgs(base::RetainedRef(this)),
        base::Milliseconds(g_dice_account_reconcilor_blocked_delay_ms));
  }

 private:
  friend class base::RefCountedThreadSafe<AccountReconcilorLockWrapper>;
  ~AccountReconcilorLockWrapper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  std::unique_ptr<AccountReconcilor::Lock> account_reconcilor_lock_;
};

// Returns true if the account reconcilor needs be be blocked while a Gaia
// sign-in request is in progress.
//
// The account reconcilor must be blocked on all request that may change the
// Gaia authentication cookies. This includes:
// * Main frame  requests.
// * XHR requests having Gaia URL as referrer.
bool ShouldBlockReconcilorForRequest(ChromeRequestAdapter* request) {
  if (request->IsOutermostMainFrame() &&
      request->GetRequestDestination() ==
          network::mojom::RequestDestination::kDocument) {
    return true;
  }

  return request->IsFetchLikeAPI() &&
         gaia::HasGaiaSchemeHostPort(request->GetReferrer());
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class RequestDestructionObserverUserData : public base::SupportsUserData::Data {
 public:
  explicit RequestDestructionObserverUserData(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  RequestDestructionObserverUserData(
      const RequestDestructionObserverUserData&) = delete;
  RequestDestructionObserverUserData& operator=(
      const RequestDestructionObserverUserData&) = delete;

  ~RequestDestructionObserverUserData() override { std::move(closure_).Run(); }

 private:
  base::OnceClosure closure_;
};

// This user data is used as a marker that a Mirror header was found on the
// redirect chain. It does not contain any data, its presence is enough to
// indicate that a header has already be found on the request.
class ManageAccountsHeaderReceivedUserData
    : public base::SupportsUserData::Data {};

#if BUILDFLAG(ENABLE_MIRROR)
// Processes the mirror response header on the UI thread. Currently depending
// on the value of |header_value|, it either shows the profile avatar menu, or
// opens an incognito window/tab.
void ProcessMirrorHeader(
    ManageAccountsParams manage_accounts_params,
    const content::WebContents::Getter& web_contents_getter,
    const std::optional<url::Origin>& request_initiator) {
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

  // Do not allow non-Google origins to open incognito windows.
  // TODO(crbug.com/40064889): Expand this check to all Mirror headers,
  //                          regardless of `service_type`.
  if (service_type == GAIA_SERVICE_TYPE_INCOGNITO &&
      base::FeatureList::IsEnabled(kVerifyRequestInitiatorForMirrorHeaders)) {
    GURL initiator_url =
        request_initiator ? request_initiator->GetURL() : GURL();
    bool is_request_initiated_by_google_domain =
        IsGoogleDomainUrl(initiator_url, google_util::ALLOW_SUBDOMAIN,
                          google_util::ALLOW_NON_STANDARD_PORTS) ||
        IsYoutubeDomainUrl(initiator_url, google_util::ALLOW_SUBDOMAIN,
                           google_util::ALLOW_NON_STANDARD_PORTS);
    base::UmaHistogramBoolean(
        "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito",
        is_request_initiated_by_google_domain);
    if (!is_request_initiated_by_google_domain) {
      VLOG(1) << "Mirror header with GAIA_SERVICE_TYPE_INCOGNITO from "
              << "untrusted domain (" << initiator_url << "), ignoring";
      return;
    }
  }

  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  account_reconcilor->OnReceivedManageAccountsResponse(service_type);

#if BUILDFLAG(IS_CHROMEOS)
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      account_reconcilor->GetState());

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  // Do not do anything if the navigation happened in the "background".
  if (!browser || !browser->window()->IsActive()) {
    return;
  }

  // Record the service type.
  base::UmaHistogramEnumeration("AccountManager.ManageAccountsServiceType",
                                service_type);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ignore response to background request from another profile, so dialogs are
  // not displayed in the wrong profile when using ChromeOS multiprofile mode.
  if (profile != ProfileManager::GetActiveUserProfile())
    return;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // The only allowed operations are:
  // 1. Going Incognito.
  // 2. Displaying a reauthentication window: Enterprise GSuite Accounts could
  //    have been forced through an online in-browser sign-in for sensitive
  //    webpages, thereby decreasing their session validity. After their session
  //    expires, they will receive a "Mirror" re-authentication request for all
  //    Google web properties. Another case when this can be triggered is
  //    https://crbug.com/1012649.
  // 3. Displaying an account addition window: when user clicks "Add another
  //    account" in One Google Bar.
  // 4. Displaying the Account Manager for managing accounts.

  // 1. Going incognito.
  if (service_type == GAIA_SERVICE_TYPE_INCOGNITO) {
    chrome::NewIncognitoWindow(profile);
    return;
  }

  // 2. Displaying a reauthentication window
  if (!manage_accounts_params.email.empty()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Do not display the re-authentication dialog if this event was triggered
    // by supervision being enabled for an account.  In this situation, a
    // complete signout is required.
    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    if (service && service->signout_required_after_supervision_enabled()) {
      return;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    // Child users shouldn't get the re-authentication dialog for primary
    // account. Log out all accounts to re-mint the cookies.
    // (See the reason below.)
    signin::IdentityManager* const identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (profile->IsChild() &&
        gaia::AreEmailsSame(primary_account.email,
                            manage_accounts_params.email)) {
      identity_manager->GetAccountsCookieMutator()->LogOutAllAccounts(
          gaia::GaiaSource::kChromeOS, base::DoNothing());
      return;
    }

    // Display a re-authentication dialog.
    signin_ui_util::ShowReauthForAccount(
        profile, manage_accounts_params.email,
        signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);
    return;
  }

  // 3. Displaying an account addition window.
  if (service_type == GAIA_SERVICE_TYPE_ADDSESSION) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    signin::IdentityManager* const identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            primary_account.account_id)) {
      // On Lacros, it is not allowed to add a new account while the primary
      // account is in error, as the reconcilor cannot generate the cookie until
      // the primary account is fixed. Display a reauth dialog instead.
      signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
          profile, signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);
      return;
    }

    AccountProfileMapper* mapper =
        g_browser_process->profile_manager()->GetAccountProfileMapper();
    SigninManagerFactory::GetForProfile(profile)->StartLacrosSigninFlow(
        profile->GetPath(), mapper,
        account_reconcilor->GetConsistencyCookieManager(),
        account_manager::AccountManagerFacade::AccountAdditionSource::
            kOgbAddAccount);
#else
    ::GetAccountManagerFacade(profile->GetPath().value())
        ->ShowAddAccountDialog(account_manager::AccountManagerFacade::
                                   AccountAdditionSource::kOgbAddAccount);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    return;
  }

  // 4. Displaying the Account Manager for managing accounts.
  ::GetAccountManagerFacade(profile->GetPath().value())
      ->ShowManageAccountsSettings();
  return;

#elif BUILDFLAG(IS_ANDROID)
  if (manage_accounts_params.show_consistency_promo) {
    SigninBridge::OpenAccountPickerBottomSheet(
        web_contents, manage_accounts_params.continue_url.empty()
                          ? chrome::kChromeUINativeNewTabURL
                          : manage_accounts_params.continue_url);
    return;
  }
  if (service_type == signin::GAIA_SERVICE_TYPE_INCOGNITO) {
    GURL url(manage_accounts_params.continue_url.empty()
                 ? chrome::kChromeUINativeNewTabURL
                 : manage_accounts_params.continue_url);
    web_contents->OpenURL(
        content::OpenURLParams(url, content::Referrer(),
                               WindowOpenDisposition::OFF_THE_RECORD,
                               ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false),
        /*navigation_handle_callback=*/{});
  } else {
    signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
        account_reconcilor->GetState());
    auto* window = web_contents->GetNativeView()->GetWindowAndroid();
    if (!window)
      return;
    SigninBridge::OpenAccountManagementScreen(window, service_type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}
#endif  // BUILDFLAG(ENABLE_MIRROR)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

void ProcessDiceHeader(
    const DiceResponseParams& dice_params,
    const content::WebContents::Getter& web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(!profile->IsOffTheRecord());

  // Ignore Dice response headers if Dice is not enabled.
  if (!AccountConsistencyModeManager::IsDiceEnabledForProfile(profile))
    return;

  DiceResponseHandler* dice_response_handler =
      DiceResponseHandlerFactory::GetForProfile(profile);
  dice_response_handler->ProcessDiceHeader(
      dice_params, ProcessDiceHeaderDelegateImpl::Create(web_contents));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_MIRROR)
// Looks for the X-Chrome-Manage-Accounts response header, and if found,
// tries to show the avatar bubble in the browser identified by the
// child/route id. Must be called on IO thread.
void ProcessMirrorResponseHeaderIfExists(ResponseAdapter* response,
                                         bool is_off_the_record) {
  CHECK(gaia::HasGaiaSchemeHostPort(response->GetUrl()));

  if (!response->IsOutermostMainFrame())
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
    NOTREACHED_IN_MIGRATION()
        << "Gaia should not send the X-Chrome-Manage-Accounts header "
        << "in incognito.";
    return;
  }

  ManageAccountsParams params = BuildManageAccountsParams(header_value);
  // If the request does not have a response header or if the header contains
  // garbage, then |service_type| is set to |GAIA_SERVICE_TYPE_NONE|.
  if (params.service_type == GAIA_SERVICE_TYPE_NONE)
    return;

  // Only process one mirror header per request (multiple headers on the same
  // redirect chain are ignored).
  if (response->GetUserData(kManageAccountsHeaderReceivedUserDataKey)) {
    LOG(ERROR) << "Multiple X-Chrome-Manage-Accounts headers on a redirect "
               << "chain, ignoring";
    return;
  }

  response->SetUserData(
      kManageAccountsHeaderReceivedUserDataKey,
      std::make_unique<ManageAccountsHeaderReceivedUserData>());

  // Post a task even if we are already on the UI thread to avoid making any
  // requests while processing a throttle event.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(ProcessMirrorHeader, params,
                                response->GetWebContentsGetter(),
                                response->GetRequestInitiator()));
}
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProcessDiceResponseHeaderIfExists(ResponseAdapter* response,
                                       bool is_off_the_record) {
  CHECK(gaia::HasGaiaSchemeHostPort(response->GetUrl()));

  if (is_off_the_record)
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

  // Post a task even if we are already on the UI thread to avoid making any
  // requests while processing a throttle event.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(ProcessDiceHeader, std::move(params),
                                response->GetWebContentsGetter()));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

std::string ParseGaiaIdFromRemoveLocalAccountResponseHeader(
    const net::HttpResponseHeaders* response_headers) {
  if (!response_headers)
    return std::string();

  std::string header_value;
  if (!response_headers->GetNormalizedHeader(
          kGoogleRemoveLocalAccountResponseHeader, &header_value)) {
    return std::string();
  }

  const SigninHeaderHelper::ResponseHeaderDictionary header_dictionary =
      SigninHeaderHelper::ParseAccountConsistencyResponseHeader(header_value);

  std::string gaia_id;
  const auto it =
      header_dictionary.find(kRemoveLocalAccountObfuscatedIDAttrName);
  if (it != header_dictionary.end()) {
    // The Gaia ID is wrapped in quotes.
    base::TrimString(it->second, "\"", &gaia_id);
  }
  return gaia_id;
}

void ProcessRemoveLocalAccountResponseHeaderIfExists(ResponseAdapter* response,
                                                     bool is_off_the_record) {
  CHECK(gaia::HasGaiaSchemeHostPort(response->GetUrl()));

  if (is_off_the_record)
    return;

  const std::string gaia_id =
      ParseGaiaIdFromRemoveLocalAccountResponseHeader(response->GetHeaders());

  if (gaia_id.empty())
    return;

  content::WebContents* web_contents = response->GetWebContentsGetter().Run();
  // The tab could have just closed. Technically, it would be possible to
  // refactor the code to pass around the profile by other means, but this
  // should be rare enough to be worth supporting.
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(!profile->IsOffTheRecord());

  IdentityManagerFactory::GetForProfile(profile)
      ->GetAccountsCookieMutator()
      ->RemoveLoggedOutAccountByGaiaId(gaia_id);
}

}  // namespace

ChromeRequestAdapter::ChromeRequestAdapter(
    const GURL& url,
    const net::HttpRequestHeaders& original_headers,
    net::HttpRequestHeaders* modified_headers,
    std::vector<std::string>* headers_to_remove)
    : RequestAdapter(url,
                     original_headers,
                     modified_headers,
                     headers_to_remove) {}

ChromeRequestAdapter::~ChromeRequestAdapter() = default;

ResponseAdapter::ResponseAdapter() = default;

ResponseAdapter::~ResponseAdapter() = default;

void SetDiceAccountReconcilorBlockDelayForTesting(int delay_ms) {
  g_dice_account_reconcilor_blocked_delay_ms = delay_ms;
}

void FixAccountConsistencyRequestHeader(
    ChromeRequestAdapter* request,
    const GURL& redirect_url,
    bool is_off_the_record,
    int incognito_availibility,
    AccountConsistencyMethod account_consistency,
    const std::string& gaia_id,
    signin::Tribool is_child_account,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    bool is_secondary_account_addition_allowed,
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    bool is_sync_enabled,
    const std::string& signin_scoped_device_id,
#endif
    content_settings::CookieSettings* cookie_settings) {
  if (is_off_the_record)
    return;  // Account consistency is disabled in incognito.

  // If new url is eligible to have the header, add it, otherwise remove it.

  // Mirror header:
  // The Mirror header may be added on desktop platforms, for integration with
  // Google Drive.
  int profile_mode_mask = PROFILE_MODE_DEFAULT;
  if (incognito_availibility ==
          static_cast<int>(policy::IncognitoModeAvailability::kDisabled) ||
      IncognitoModePrefs::ArePlatformParentalControlsEnabled()) {
    profile_mode_mask |= PROFILE_MODE_INCOGNITO_DISABLED;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!is_secondary_account_addition_allowed) {
    account_consistency = AccountConsistencyMethod::kMirror;
    // Can't add new accounts.
    profile_mode_mask |= PROFILE_MODE_ADD_ACCOUNT_DISABLED;
  }
#endif

  AppendOrRemoveMirrorRequestHeader(
      request, redirect_url, gaia_id, is_child_account, account_consistency,
      cookie_settings, profile_mode_mask, kChromeMirrorHeaderSource,
      /*force_account_consistency=*/false);

// Dice header:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bool dice_header_added = AppendOrRemoveDiceRequestHeader(
      request, redirect_url, gaia_id, is_sync_enabled, account_consistency,
      cookie_settings, signin_scoped_device_id);

  // Block the AccountReconcilor while the Dice requests are in flight. This
  // allows the DiceReponseHandler to process the response before the reconcilor
  // starts.
  if (dice_header_added && ShouldBlockReconcilorForRequest(request)) {
    auto lock_wrapper = base::MakeRefCounted<AccountReconcilorLockWrapper>(
        request->GetWebContentsGetter());
    // On destruction of the request |lock_wrapper| will be released.
    request->SetDestructionCallback(base::BindOnce(
        &AccountReconcilorLockWrapper::DestroyAfterDelay, lock_wrapper));
  }
#endif
}

void ProcessAccountConsistencyResponseHeaders(ResponseAdapter* response,
                                              const GURL& redirect_url,
                                              bool is_off_the_record) {
  if (!gaia::HasGaiaSchemeHostPort(response->GetUrl())) {
    return;
  }

#if BUILDFLAG(ENABLE_MIRROR)
  // See if the response contains the X-Chrome-Manage-Accounts header. If so
  // show the profile avatar bubble so that user can complete signin/out
  // action the native UI.
  ProcessMirrorResponseHeaderIfExists(response, is_off_the_record);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Process the Dice header: on sign-in, exchange the authorization code for a
  // refresh token, on sign-out just follow the sign-out URL.
  ProcessDiceResponseHeaderIfExists(response, is_off_the_record);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  ProcessRemoveLocalAccountResponseHeaderIfExists(response, is_off_the_record);
}

std::string ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
    const net::HttpResponseHeaders* response_headers) {
  return ParseGaiaIdFromRemoveLocalAccountResponseHeader(response_headers);
}

}  // namespace signin
