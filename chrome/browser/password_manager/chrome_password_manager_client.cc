// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/form_submission_tracker_util.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/content/browser/password_requirements_service_factory.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/store_metrics_reporter.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/url_constants.h"

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/password_manager/account_chooser_dialog_android.h"
#include "chrome/browser/password_manager/auto_signin_first_run_dialog_android.h"
#include "chrome/browser/password_manager/generated_password_saved_infobar_delegate_android.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/browser/password_manager/save_password_infobar_delegate_android.h"
#include "chrome/browser/password_manager/update_password_infobar_delegate_android.h"
#include "chrome/browser/ui/android/snackbars/auto_signin_prompt_controller.h"
#include "ui/base/ui_base_features.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

using password_manager::metrics_util::PasswordType;
using password_manager::BadMessageReason;
using password_manager::ContentPasswordManagerDriverFactory;
using password_manager::PasswordManagerClientHelper;
using password_manager::PasswordManagerInternalsService;
using password_manager::PasswordManagerMetricsRecorder;
using sessions::SerializedNavigationEntry;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace {

const syncer::SyncService* GetSyncService(Profile* profile) {
  if (ProfileSyncServiceFactory::HasProfileSyncService(profile))
    return ProfileSyncServiceFactory::GetForProfile(profile);
  return nullptr;
}

const SigninManagerBase* GetSigninManagerForOriginalProfile(Profile* profile) {
  return SigninManagerFactory::GetForProfile(profile->GetOriginalProfile());
}

#if !defined(OS_ANDROID)
// Adds |observer| to the input observers of |widget_host|.
void AddToWidgetInputEventObservers(
    content::RenderWidgetHost* widget_host,
    content::RenderWidgetHost::InputEventObserver* observer) {
  // Since Widget API doesn't allow to check whether the observer is already
  // added, the observer is removed and added again, to ensure that it is added
  // only once.
  widget_host->RemoveInputEventObserver(observer);
  widget_host->AddInputEventObserver(observer);
}
#endif

}  // namespace

// static
void ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
    content::WebContents* contents,
    autofill::AutofillClient* autofill_client) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(UserDataKey(),
                        base::WrapUnique(new ChromePasswordManagerClient(
                            contents, autofill_client)));
}

ChromePasswordManagerClient::ChromePasswordManagerClient(
    content::WebContents* web_contents,
    autofill::AutofillClient* autofill_client)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      password_manager_(this),
// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID)
      password_reuse_detection_manager_(this),
#endif
      driver_factory_(nullptr),
      content_credential_manager_(this),
      password_manager_client_bindings_(web_contents, this),
      password_manager_driver_bindings_(web_contents, this),
      observer_(nullptr),
      credentials_filter_(
          this,
          base::BindRepeating(&GetSyncService, profile_),
          base::BindRepeating(&GetSigninManagerForOriginalProfile, profile_)),
      helper_(this) {
  ContentPasswordManagerDriverFactory::CreateForWebContents(web_contents, this,
                                                            autofill_client);
  driver_factory_ =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents);
  log_manager_ = password_manager::LogManager::Create(
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(profile_),
      base::Bind(
          &ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability,
          base::Unretained(driver_factory_)));

  saving_and_filling_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, GetPrefs());
  static base::NoDestructor<password_manager::StoreMetricsReporter> reporter(
      *saving_and_filling_passwords_enabled_, this, GetSyncService(profile_),
      GetSigninManagerForOriginalProfile(profile_), GetPrefs());
  driver_factory_->RequestSendLoggingAvailability();
}

ChromePasswordManagerClient::~ChromePasswordManagerClient() {}

bool ChromePasswordManagerClient::IsPasswordManagementEnabledForCurrentPage()
    const {
  DCHECK(web_contents());
  content::NavigationEntry* entry = nullptr;
  switch (password_manager_.entry_to_check()) {
    case password_manager::PasswordManager::NavigationEntryToCheck::
        LAST_COMMITTED:
      entry = web_contents()->GetController().GetLastCommittedEntry();
      break;
    case password_manager::PasswordManager::NavigationEntryToCheck::VISIBLE:
      entry = web_contents()->GetController().GetVisibleEntry();
      break;
  }
  bool is_enabled = false;
  if (!entry) {
    // TODO(gcasto): Determine if fix for crbug.com/388246 is relevant here.
    is_enabled = true;
  } else {
    is_enabled = CanShowBubbleOnURL(entry->GetURL());
  }

  // The password manager is disabled while VR (virtual reality) is being used,
  // as the use of conventional UI elements might harm the user experience in
  // VR.
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          web_contents(), vr::UiSuppressedElement::kPasswordManager)) {
    is_enabled = false;
  }

  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogBoolean(
        Logger::STRING_PASSWORD_MANAGEMENT_ENABLED_FOR_CURRENT_PAGE,
        is_enabled);
  }
  return is_enabled;
}

bool ChromePasswordManagerClient::IsSavingAndFillingEnabledForCurrentPage()
    const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    // Disable the password saving UI for automated tests. It obscures the
    // page, and there is no API to access (or dismiss) UI bubbles/infobars.
    return false;
  }
  // TODO(melandory): remove saving_and_filling_passwords_enabled_ check from
  // here once we decide to switch to new settings behavior for everyone.
  return *saving_and_filling_passwords_enabled_ && !IsIncognito() &&
         IsFillingEnabledForCurrentPage();
}

bool ChromePasswordManagerClient::IsFillingEnabledForCurrentPage() const {
  const bool ssl_errors = net::IsCertStatusError(GetMainFrameCertStatus());

  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogBoolean(Logger::STRING_SSL_ERRORS_PRESENT, ssl_errors);
  }

  return !ssl_errors && IsPasswordManagementEnabledForCurrentPage();
}

bool ChromePasswordManagerClient::IsFillingFallbackEnabledForCurrentPage()
    const {
  return IsFillingEnabledForCurrentPage() &&
         !Profile::FromBrowserContext(web_contents()->GetBrowserContext())
              ->IsGuestSession();
}

void ChromePasswordManagerClient::PostHSTSQueryForHost(
    const GURL& origin,
    password_manager::HSTSCallback callback) const {
  password_manager::PostHSTSQueryForHostAndNetworkContext(
      origin,
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetNetworkContext(),
      std::move(callback));
}

bool ChromePasswordManagerClient::OnCredentialManagerUsed() {
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents());
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_CREDENTIAL_MANAGER_API);
    return false;
  }
  return true;
}

bool ChromePasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  // Save password infobar and the password bubble prompts in case of
  // "webby" URLs and do not prompt in case of "non-webby" URLS (e.g. file://).
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return false;

#if !defined(OS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  if (update_password) {
    manage_passwords_ui_controller->OnUpdatePasswordSubmitted(
        std::move(form_to_save));
  } else {
    manage_passwords_ui_controller->OnPasswordSubmitted(
        std::move(form_to_save));
  }
#else
  if (form_to_save->IsBlacklisted())
    return false;

  if (update_password) {
    UpdatePasswordInfoBarDelegate::Create(web_contents(),
                                          std::move(form_to_save));
  } else {
    SavePasswordInfoBarDelegate::Create(web_contents(),
                                        std::move(form_to_save));
  }
#endif  // !defined(OS_ANDROID)
  return true;
}

void ChromePasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return;

#if !defined(OS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  // There may be no UI controller for ChromeOS login page
  // (see crbug.com/774676).
  if (manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnShowManualFallbackForSaving(
        std::move(form_to_save), has_generated_password, is_update);
  }
#endif  // !defined(OS_ANDROID)
}

void ChromePasswordManagerClient::HideManualFallbackForSaving() {
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return;

#if !defined(OS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  // There may be no UI controller for ChromeOS login page
  // (see crbug.com/774676).
  if (manage_passwords_ui_controller)
    manage_passwords_ui_controller->OnHideManualFallbackForSaving();
#endif  // !defined(OS_ANDROID)
}

bool ChromePasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin,
    const CredentialsCallback& callback) {
  // Set up an intercept callback if the prompt is zero-clickable (e.g. just one
  // form provided).
  CredentialsCallback intercept =
      base::Bind(&PasswordManagerClientHelper::OnCredentialsChosen,
                 base::Unretained(&helper_), callback, local_forms.size() == 1);
#if defined(OS_ANDROID)
  // Deletes itself on the event from Java counterpart, when user interacts with
  // dialog.
  AccountChooserDialogAndroid* acccount_chooser_dialog =
      new AccountChooserDialogAndroid(web_contents(), std::move(local_forms),
                                      origin, intercept);
  acccount_chooser_dialog->ShowDialog();
  return true;
#else
  return PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnChooseCredentials(std::move(local_forms), origin, intercept);
#endif
}

void ChromePasswordManagerClient::GeneratePassword() {
  password_manager::ContentPasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(web_contents()->GetFocusedFrame());
  driver->GeneratePassword();
}

void ChromePasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {
  DCHECK(!local_forms.empty());
  helper_.NotifyUserAutoSignin();
#if defined(OS_ANDROID)
  ShowAutoSigninPrompt(web_contents(), local_forms[0]->username_value);
#else
  PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnAutoSignin(std::move(local_forms), origin);
#endif
}

void ChromePasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {
  helper_.NotifyUserCouldBeAutoSignedIn(std::move(form));
}

void ChromePasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    const autofill::PasswordForm& form) {
  helper_.NotifySuccessfulLoginWithExistingPassword(form);
}

void ChromePasswordManagerClient::NotifyStorePasswordCalled() {
  helper_.NotifyStorePasswordCalled();
  was_store_ever_called_ = true;
}

void ChromePasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form) {
#if defined(OS_ANDROID)
  GeneratedPasswordSavedInfoBarDelegateAndroid::Create(web_contents());
#else
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnAutomaticPasswordSave(
      std::move(saved_form));
#endif
}

void ChromePasswordManagerClient::PasswordWasAutofilled(
    const std::map<base::string16, const autofill::PasswordForm*>& best_matches,
    const GURL& origin,
    const std::vector<const autofill::PasswordForm*>* federated_matches) const {
#if defined(OS_ANDROID)
  // Either #passwords-keyboards-accessory or #experimental-ui must be enabled.
  if (!PasswordAccessoryController::AllowedForWebContents(web_contents())) {
    return;  // No need to even create the bridge if it's not going to be used.
  }
  // If an accessory exists already, |CreateForWebContents| is a NoOp
  PasswordAccessoryController::CreateForWebContents(web_contents());
  PasswordAccessoryController::FromWebContents(web_contents())
      ->SavePasswordsForOrigin(best_matches, url::Origin::Create(origin));
#else  // !defined(OS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnPasswordAutofilled(best_matches, origin,
                                                       federated_matches);
#endif
}

void ChromePasswordManagerClient::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {
#if defined(SAFE_BROWSING_DB_LOCAL)
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (pps) {
    pps->MaybeStartPasswordFieldOnFocusRequest(
        web_contents(), GetMainFrameURL(), form_action, frame_url);
  }
#endif
}

void ChromePasswordManagerClient::HidePasswordGenerationPopup() {
  if (popup_controller_)
    popup_controller_->HideAndDestroy();
}

#if defined(SAFE_BROWSING_DB_LOCAL)
safe_browsing::PasswordProtectionService*
ChromePasswordManagerClient::GetPasswordProtectionService() const {
  return safe_browsing::ChromePasswordProtectionService::
      GetPasswordProtectionService(profile_);
}

void ChromePasswordManagerClient::CheckProtectedPasswordEntry(
    PasswordType reused_password_type,
    const std::vector<std::string>& matching_domains,
    bool password_field_exists) {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (!pps)
    return;
  pps->MaybeStartProtectedPasswordEntryRequest(
      web_contents(), GetMainFrameURL(),
      safe_browsing::PasswordProtectionService::
          GetPasswordProtectionReusedPasswordType(reused_password_type),
      matching_domains, password_field_exists);
}

void ChromePasswordManagerClient::LogPasswordReuseDetectedEvent() {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (pps) {
    pps->MaybeLogPasswordReuseDetectedEvent(web_contents());
  }
}
#endif

ukm::SourceId ChromePasswordManagerClient::GetUkmSourceId() {
  return ukm::GetSourceIdForWebContentsDocument(web_contents());
}

PasswordManagerMetricsRecorder*
ChromePasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(GetUkmSourceId(), GetMainFrameURL());
  }
  return base::OptionalOrNullptr(metrics_recorder_);
}

void ChromePasswordManagerClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  if (!navigation_handle->IsSameDocument()) {
    // Send any collected metrics by destroying the metrics recorder.
    metrics_recorder_.reset();
  }

  // From this point on, the ContentCredentialManager will service API calls in
  // the context of the new WebContents::GetLastCommittedURL, which may very
  // well be cross-origin. Disconnect existing client, and drop pending
  // requests.
  if (!navigation_handle->IsSameDocument())
    content_credential_manager_.DisconnectBinding();

// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID)
  password_reuse_detection_manager_.DidNavigateMainFrame(GetMainFrameURL());
  AddToWidgetInputEventObservers(
      web_contents()->GetRenderViewHost()->GetWidget(), this);
#else   // defined(OS_ANDROID)
  PasswordAccessoryController* accessory =
      PasswordAccessoryController::FromWebContents(web_contents());
  if (accessory)
    accessory->DidNavigateMainFrame();
#endif  // defined(OS_ANDROID)
}

#if !defined(OS_ANDROID)
void ChromePasswordManagerClient::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // TODO(drubery): We should handle input events on subframes separately, so
  // that we can accurately report that the password was reused on a subframe.
  // Currently any password reuse for this WebContents will report password
  // reuse on the main frame URL.
  AddToWidgetInputEventObservers(
      render_frame_host->GetView()->GetRenderWidgetHost(), this);
}
#endif

#if !defined(OS_ANDROID)
void ChromePasswordManagerClient::OnInputEvent(
    const blink::WebInputEvent& event) {
  if (event.GetType() != blink::WebInputEvent::kChar)
    return;
  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);
  password_reuse_detection_manager_.OnKeyPressed(key_event.text);
}
#endif

PrefService* ChromePasswordManagerClient::GetPrefs() const {
  return profile_->GetPrefs();
}

password_manager::PasswordStore*
ChromePasswordManagerClient::GetPasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  // TODO(gcasto): Is is safe to change this to
  // ServiceAccessType::IMPLICIT_ACCESS?
  return PasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS).get();
}

password_manager::SyncState ChromePasswordManagerClient::GetPasswordSyncState()
    const {
  const browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return password_manager_util::GetPasswordSyncState(sync_service);
}

bool ChromePasswordManagerClient::WasLastNavigationHTTPError() const {
  DCHECK(web_contents());

  std::unique_ptr<password_manager::BrowserSavePasswordProgressLogger> logger;
  if (log_manager_->IsLoggingActive()) {
    logger.reset(new password_manager::BrowserSavePasswordProgressLogger(
        log_manager_.get()));
    logger->LogMessage(
        Logger::STRING_WAS_LAST_NAVIGATION_HTTP_ERROR_METHOD);
  }

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry)
    return false;
  int http_status_code = entry->GetHttpStatusCode();

  if (logger)
    logger->LogNumber(Logger::STRING_HTTP_STATUS_CODE, http_status_code);

  if (http_status_code >= 400 && http_status_code < 600)
    return true;
  return false;
}

net::CertStatus ChromePasswordManagerClient::GetMainFrameCertStatus() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return 0;
  return entry->GetSSL().cert_status;
}

bool ChromePasswordManagerClient::IsIncognito() const {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

const password_manager::PasswordManager*
ChromePasswordManagerClient::GetPasswordManager() const {
  return &password_manager_;
}

autofill::AutofillManager*
ChromePasswordManagerClient::GetAutofillManagerForMainFrame() {
  autofill::ContentAutofillDriverFactory* factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents());
  if (factory) {
    autofill::ContentAutofillDriver* driver =
        factory->DriverForFrame(web_contents()->GetMainFrame());
    // |driver| can be NULL if the tab is being closed.
    if (driver)
      return driver->autofill_manager();
  }
  return nullptr;
}

void ChromePasswordManagerClient::SetTestObserver(
    PasswordGenerationPopupObserver* observer) {
  observer_ = observer;
}

void ChromePasswordManagerClient::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Logging has no sense on WebUI sites.
  log_manager_->SetSuspended(web_contents()->GetWebUI() != nullptr);
  int current_process_id =
      navigation_handle->GetStartingSiteInstance()->GetProcess()->GetID();
  content::RenderFrameHost* current_frame_host =
      web_contents()->FindFrameByFrameTreeNodeId(
          navigation_handle->GetFrameTreeNodeId(), current_process_id);
  password_manager::NotifyOnStartNavigation(
      navigation_handle, driver_factory_->GetDriverForFrame(current_frame_host),
      &password_manager_);
}

gfx::RectF ChromePasswordManagerClient::GetBoundsInScreenSpace(
    const gfx::RectF& bounds) {
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  return bounds + client_area.OffsetFromOrigin();
}

void ChromePasswordManagerClient::AutomaticGenerationStatusChanged(
    bool available,
    const base::Optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data) {
  if (ui_data &&
      !password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_client_bindings_.GetCurrentTargetFrame(),
          ui_data->password_form,
          BadMessageReason::
              CPMD_BAD_ORIGIN_AUTOMATIC_GENERATION_STATUS_CHANGED))
    return;
#if defined(OS_ANDROID)
  // Either #passwords-keyboards-accessory or #experimental-ui must be enabled.
  if (PasswordAccessoryController::AllowedForWebContents(web_contents())) {
    if (available) {
      PasswordAccessoryController::CreateForWebContents(web_contents());
      password_manager::PasswordManagerDriver* driver =
          driver_factory_->GetDriverForFrame(
              password_manager_client_bindings_.GetCurrentTargetFrame());
      DCHECK(driver);
      password_manager_.SetGenerationElementAndReasonForForm(
          driver, ui_data.value().password_form,
          ui_data.value().generation_element,
          false /* is_manually_triggered */);
      PasswordAccessoryController::FromWebContents(web_contents())
          ->OnAutomaticGenerationStatusChanged(true, ui_data,
                                               driver->AsWeakPtr());
      gfx::RectF element_bounds_in_screen_space = TransformToRootCoordinates(
          password_manager_client_bindings_.GetCurrentTargetFrame(),
          ui_data.value().bounds);
      driver->GetPasswordAutofillManager()->MaybeShowPasswordSuggestions(
          element_bounds_in_screen_space, ui_data.value().text_direction);
    } else {
      PasswordAccessoryController* accessory =
          PasswordAccessoryController::FromWebContents(web_contents());
      if (accessory) {
        accessory->OnAutomaticGenerationStatusChanged(false, base::nullopt,
                                                      nullptr);
      }
    }
  }
#else
  if (available) {
    ShowPasswordGenerationPopup(ui_data.value(),
                                false /* is_manually_triggered */);
  }
#endif  // defined(OS_ANDROID)
}

void ChromePasswordManagerClient::ShowManualPasswordGenerationPopup(
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_client_bindings_.GetCurrentTargetFrame(),
          ui_data.password_form,
          BadMessageReason::
              CPMD_BAD_ORIGIN_SHOW_MANUAL_PASSWORD_GENERATION_POPUP))
    return;
  ShowPasswordGenerationPopup(ui_data, true /* is_manually_triggered */);
}

void ChromePasswordManagerClient::ShowPasswordEditingPopup(
    const gfx::RectF& bounds,
    const autofill::PasswordForm& form) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_client_bindings_.GetCurrentTargetFrame(), form,
          BadMessageReason::CPMD_BAD_ORIGIN_SHOW_PASSWORD_EDITING_POPUP))
    return;
  auto* driver = driver_factory_->GetDriverForFrame(
      password_manager_client_bindings_.GetCurrentTargetFrame());
  DCHECK(driver);
  gfx::RectF element_bounds_in_screen_space =
      GetBoundsInScreenSpace(TransformToRootCoordinates(
          password_manager_driver_bindings_.GetCurrentTargetFrame(), bounds));
  popup_controller_ = PasswordGenerationPopupControllerImpl::GetOrCreate(
      popup_controller_, element_bounds_in_screen_space, form,
      base::string16(),  // No generation_element needed for editing.
      0,                 // Unspecified max length.
      &password_manager_, driver->AsWeakPtr(), observer_, web_contents(),
      web_contents()->GetNativeView());
  DCHECK(!form.password_value.empty());
  popup_controller_->UpdatePassword(form.password_value);
  popup_controller_->Show(
      PasswordGenerationPopupController::kEditGeneratedPassword);
}

void ChromePasswordManagerClient::GenerationAvailableForForm(
    const autofill::PasswordForm& form) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_client_bindings_.GetCurrentTargetFrame(), form,
          BadMessageReason::CPMD_BAD_ORIGIN_GENERATION_AVAILABLE_FOR_FORM))
    return;
  password_manager_.GenerationAvailableForForm(form);
}

void ChromePasswordManagerClient::PasswordGenerationRejectedByTyping() {
  // TODO(crbug.com/835234):The call to hide the popup should be made for
  // desktop only.
  HidePasswordGenerationPopup();
}

void ChromePasswordManagerClient::PresaveGeneratedPassword(
    const autofill::PasswordForm& password_form) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_client_bindings_.GetCurrentTargetFrame(),
          password_form,
          BadMessageReason::CPMD_BAD_ORIGIN_PRESAVE_GENERATED_PASSWORD))
    return;
  if (popup_controller_)
    popup_controller_->UpdatePassword(password_form.password_value);

  password_manager::PasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_manager_driver_bindings_.GetCurrentTargetFrame());
  password_manager_.OnPresaveGeneratedPassword(driver, password_form);
}

void ChromePasswordManagerClient::PasswordNoLongerGenerated(
    const autofill::PasswordForm& password_form) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_client_bindings_.GetCurrentTargetFrame(),
          password_form,
          BadMessageReason::CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED))
    return;

  password_manager::PasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_manager_driver_bindings_.GetCurrentTargetFrame());
  password_manager_.OnPasswordNoLongerGenerated(driver, password_form);

  PasswordGenerationPopupController* controller = popup_controller_.get();
  if (controller &&
      controller->state() ==
          PasswordGenerationPopupController::kEditGeneratedPassword) {
    HidePasswordGenerationPopup();
  }
}

const GURL& ChromePasswordManagerClient::GetMainFrameURL() const {
  return web_contents()->GetVisibleURL();
}

bool ChromePasswordManagerClient::IsMainFrameSecure() const {
  return content::IsOriginSecure(web_contents()->GetVisibleURL());
}

const GURL& ChromePasswordManagerClient::GetLastCommittedEntryURL() const {
  DCHECK(web_contents());
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return GURL::EmptyGURL();

  return entry->GetURL();
}

// static
bool ChromePasswordManagerClient::ShouldAnnotateNavigationEntries(
    Profile* profile) {
  // Only annotate PasswordState onto the navigation entry if user is
  // opted into UMA and they're not syncing w/ a custom passphrase.
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled())
    return false;

  browser_sync::ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!profile_sync_service || !profile_sync_service->IsSyncFeatureActive() ||
      profile_sync_service->IsUsingSecondaryPassphrase()) {
    return false;
  }

  return true;
}

void ChromePasswordManagerClient::AnnotateNavigationEntry(
    bool has_password_field) {
  if (!ShouldAnnotateNavigationEntries(profile_))
    return;

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  SerializedNavigationEntry::PasswordState old_state =
      sessions::GetPasswordStateFromNavigation(*entry);

  SerializedNavigationEntry::PasswordState new_state =
      (has_password_field ? SerializedNavigationEntry::HAS_PASSWORD_FIELD
                          : SerializedNavigationEntry::NO_PASSWORD_FIELD);

  if (new_state > old_state) {
    SetPasswordStateInNavigation(new_state, entry);
  }
}

const password_manager::CredentialsFilter*
ChromePasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

const password_manager::LogManager* ChromePasswordManagerClient::GetLogManager()
    const {
  return log_manager_.get();
}

password_manager::PasswordRequirementsService*
ChromePasswordManagerClient::GetPasswordRequirementsService() {
  return password_manager::PasswordRequirementsServiceFactory::
      GetForBrowserContext(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

favicon::FaviconService* ChromePasswordManagerClient::GetFaviconService() {
  return FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

void ChromePasswordManagerClient::UpdateFormManagers() {
  password_manager_.UpdateFormManagers();
}

bool ChromePasswordManagerClient::IsUnderAdvancedProtection() const {
#if defined(FULL_SAFE_BROWSING)
  return safe_browsing::AdvancedProtectionStatusManager::
      IsUnderAdvancedProtection(profile_);
#else
  return false;
#endif
}

void ChromePasswordManagerClient::PasswordFormsParsed(
    const std::vector<autofill::PasswordForm>& forms) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_driver_bindings_.GetCurrentTargetFrame(), forms,
          BadMessageReason::CPMD_BAD_ORIGIN_FORMS_PARSED))
    return;
  password_manager::PasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_manager_driver_bindings_.GetCurrentTargetFrame());
  GetPasswordManager()->OnPasswordFormsParsed(driver, forms);
}

void ChromePasswordManagerClient::PasswordFormsRendered(
    const std::vector<autofill::PasswordForm>& visible_forms,
    bool did_stop_loading) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_driver_bindings_.GetCurrentTargetFrame(),
          visible_forms, BadMessageReason::CPMD_BAD_ORIGIN_FORMS_RENDERED))
    return;
  password_manager::PasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_manager_driver_bindings_.GetCurrentTargetFrame());
  GetPasswordManager()->OnPasswordFormsRendered(driver, visible_forms,
                                                did_stop_loading);
}

void ChromePasswordManagerClient::PasswordFormSubmitted(
    const autofill::PasswordForm& password_form) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_driver_bindings_.GetCurrentTargetFrame(),
          password_form, BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED))
    return;
  password_manager::PasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_manager_driver_bindings_.GetCurrentTargetFrame());
  GetPasswordManager()->OnPasswordFormSubmitted(driver, password_form);
}

void ChromePasswordManagerClient::ShowManualFallbackForSaving(
    const autofill::PasswordForm& password_form) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_driver_bindings_.GetCurrentTargetFrame(),
          password_form,
          BadMessageReason::CPMD_BAD_ORIGIN_SHOW_FALLBACK_FOR_SAVING))
    return;
  password_manager::PasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_manager_driver_bindings_.GetCurrentTargetFrame());
  GetPasswordManager()->ShowManualFallbackForSaving(driver, password_form);
}

void ChromePasswordManagerClient::SameDocumentNavigation(
    const autofill::PasswordForm& password_form) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          password_manager_driver_bindings_.GetCurrentTargetFrame(),
          password_form, BadMessageReason::CPMD_BAD_ORIGIN_IN_PAGE_NAVIGATION))
    return;
  password_manager::PasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_manager_driver_bindings_.GetCurrentTargetFrame());
  GetPasswordManager()->OnPasswordFormSubmittedNoChecks(driver, password_form);
}

void ChromePasswordManagerClient::ShowPasswordSuggestions(
    base::i18n::TextDirection text_direction,
    const base::string16& typed_username,
    int options,
    const gfx::RectF& bounds) {
  password_manager::PasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_manager_driver_bindings_.GetCurrentTargetFrame());
  driver->GetPasswordAutofillManager()->OnShowPasswordSuggestions(
      text_direction, typed_username, options,
      TransformToRootCoordinates(
          password_manager_driver_bindings_.GetCurrentTargetFrame(), bounds));
}

void ChromePasswordManagerClient::RecordSavePasswordProgress(
    const std::string& log) {
  GetLogManager()->LogSavePasswordProgress(log);
}

void ChromePasswordManagerClient::UserModifiedPasswordField() {
  if (GetMetricsRecorder()) {
    GetMetricsRecorder()->RecordUserModifiedPasswordField();
  }
}

// static
void ChromePasswordManagerClient::BindCredentialManager(
    blink::mojom::CredentialManagerRequest request,
    content::RenderFrameHost* render_frame_host) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  // Only valid for the currently committed RenderFrameHost, and not, e.g. old
  // zombie RFH's being swapped out following cross-origin navigations.
  if (web_contents->GetMainFrame() != render_frame_host)
    return;

  ChromePasswordManagerClient* instance =
      ChromePasswordManagerClient::FromWebContents(web_contents);

  // Try to bind to the driver, but if driver is not available for this render
  // frame host, the request will be just dropped. This will cause the message
  // pipe to be closed, which will raise a connection error on the peer side.
  if (!instance)
    return;

  instance->content_credential_manager_.BindRequest(std::move(request));
}

// static
bool ChromePasswordManagerClient::CanShowBubbleOnURL(const GURL& url) {
  std::string scheme = url.scheme();
  return (content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
              scheme) &&
#if BUILDFLAG(ENABLE_EXTENSIONS)
          scheme != extensions::kExtensionScheme &&
#endif
          scheme != content::kChromeDevToolsScheme);
}

void ChromePasswordManagerClient::PromptUserToEnableAutosignin() {
#if defined(OS_ANDROID)
  // Dialog is deleted by the Java counterpart after user interacts with it.
  AutoSigninFirstRunDialogAndroid* auto_signin_first_run_dialog =
      new AutoSigninFirstRunDialogAndroid(web_contents());
  auto_signin_first_run_dialog->ShowDialog();
#else
  PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnPromptEnableAutoSignin();
#endif
}

void ChromePasswordManagerClient::ShowPasswordGenerationPopup(
    const autofill::password_generation::PasswordGenerationUIData& ui_data,
    bool is_manually_triggered) {
  auto* driver = driver_factory_->GetDriverForFrame(
      password_manager_client_bindings_.GetCurrentTargetFrame());
  DCHECK(driver);
  gfx::RectF element_bounds_in_top_frame_space = TransformToRootCoordinates(
      password_manager_client_bindings_.GetCurrentTargetFrame(),
      ui_data.bounds);
  if (!is_manually_triggered &&
      driver->GetPasswordAutofillManager()
          ->MaybeShowPasswordSuggestionsWithGeneration(
              element_bounds_in_top_frame_space, ui_data.text_direction)) {
    return;
  }

  gfx::RectF element_bounds_in_screen_space =
      GetBoundsInScreenSpace(element_bounds_in_top_frame_space);
  password_manager_.SetGenerationElementAndReasonForForm(
      driver, ui_data.password_form, ui_data.generation_element,
      is_manually_triggered);

  popup_controller_ = PasswordGenerationPopupControllerImpl::GetOrCreate(
      popup_controller_, element_bounds_in_screen_space, ui_data.password_form,
      ui_data.generation_element, ui_data.max_length, &password_manager_,
      driver->AsWeakPtr(), observer_, web_contents(),
      web_contents()->GetNativeView());
  popup_controller_->Show(PasswordGenerationPopupController::kOfferGeneration);
}

void ChromePasswordManagerClient::FocusedInputChanged(bool is_fillable,
                                                      bool is_password_field) {
#if defined(OS_ANDROID)
  // Either #passwords-keyboards-accessory or #experimental-ui must be enabled.
  if (!PasswordAccessoryController::AllowedForWebContents(web_contents())) {
    return;  // No need to even create the bridge if it's not going to be used.
  }
  if (is_password_field) {
    DCHECK(is_fillable);
    PasswordAccessoryController::CreateForWebContents(web_contents());
    PasswordAccessoryController* accessory =
        PasswordAccessoryController::FromWebContents(web_contents());
    accessory->RefreshSuggestionsForField(
        password_manager_driver_bindings_.GetCurrentTargetFrame()
            ->GetLastCommittedOrigin(),
        is_fillable, is_password_field);
    accessory->ShowWhenKeyboardIsVisible();
  } else {
    // If not a password field, only update the accessory if one exists.
    PasswordAccessoryController* accessory =
        PasswordAccessoryController::FromWebContents(web_contents());
    if (accessory) {
      accessory->Hide();
    }
  }
#endif  // defined(OS_ANDROID)
}

password_manager::PasswordManager*
ChromePasswordManagerClient::GetPasswordManager() {
  return &password_manager_;
}

gfx::RectF ChromePasswordManagerClient::TransformToRootCoordinates(
    content::RenderFrameHost* frame_host,
    const gfx::RectF& bounds_in_frame_coordinates) {
  content::RenderWidgetHostView* rwhv = frame_host->GetView();
  if (!rwhv)
    return bounds_in_frame_coordinates;
  return gfx::RectF(rwhv->TransformPointToRootCoordSpaceF(
                        bounds_in_frame_coordinates.origin()),
                    bounds_in_frame_coordinates.size());
}
