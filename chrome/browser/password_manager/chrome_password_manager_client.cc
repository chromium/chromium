// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/password_manager/field_info_manager_factory.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/password_reuse_signal.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/content/browser/password_requirements_service_factory.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/url_formatter/url_formatter.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/url_constants.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/password_manager/android/account_chooser_dialog_android.h"
#include "chrome/browser/password_manager/android/auto_signin_first_run_dialog_android.h"
#include "chrome/browser/password_manager/android/auto_signin_prompt_controller.h"
#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_controller_impl.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller_autofill_delegate.h"
#include "components/messages/android/messages_feature.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "ui/base/ui_base_features.h"
#else
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/policy/core/common/features.h"
#include "ui/events/keycodes/keyboard_codes.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/ui/browser.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/scoped_allow_sync_call.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

#if BUILDFLAG(IS_ANDROID)
using password_manager::CredentialCache;
#endif

using autofill::mojom::FocusedFieldType;
using autofill::password_generation::PasswordGenerationType;
using password_manager::BadMessageReason;
using password_manager::ContentPasswordManagerDriverFactory;
using password_manager::FieldInfoManager;
using password_manager::PasswordForm;
using password_manager::PasswordManagerClientHelper;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerMetricsRecorder;
using password_manager::PasswordManagerSetting;
using password_manager::metrics_util::PasswordType;
using sessions::SerializedNavigationEntry;
using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace {

#if !BUILDFLAG(IS_ANDROID)
static const char kPasswordBreachEntryTrigger[] = "PASSWORD_ENTRY";
constexpr char kExtensionScheme[] = "chrome-extension";
#endif

const syncer::SyncService* GetSyncServiceForProfile(Profile* profile) {
  if (SyncServiceFactory::HasSyncService(profile))
    return SyncServiceFactory::GetForProfile(profile);
  return nullptr;
}

// Adds |observer| to the input observers of |widget_host|.
void AddToWidgetInputEventObservers(
    content::RenderWidgetHost* widget_host,
    content::RenderWidgetHost::InputEventObserver* observer) {
  // Since Widget API doesn't allow to check whether the observer is already
  // added, the observer is removed and added again, to ensure that it is added
  // only once.
#if BUILDFLAG(IS_ANDROID)
  widget_host->RemoveImeInputEventObserver(observer);
  widget_host->AddImeInputEventObserver(observer);
#endif
  widget_host->RemoveInputEventObserver(observer);
  widget_host->AddInputEventObserver(observer);
}

#if !BUILDFLAG(IS_ANDROID)
// Retrieves and formats the saved passwords domains from signon_realms.
std::vector<std::string> GetMatchingDomains(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  base::flat_set<std::string> matching_domains;
  for (const auto& credential : matching_reused_credentials) {
    // TODO(zackhan): Avoid converting signon_realm to URL, ideally use
    // PasswordForm::url.
    std::string domain = base::UTF16ToUTF8(url_formatter::FormatUrl(
        GURL(credential.signon_realm),
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    matching_domains.insert(std::move(domain));
  }
  return std::move(matching_domains).extract();
}
#endif  // !BUILDFLAG(IS_ANDROID)

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

// static
void ChromePasswordManagerClient::BindPasswordGenerationDriver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordGenerationDriver>
        receiver,
    content::RenderFrameHost* rfh) {
  // [spec] https://wicg.github.io/anonymous-iframe/#spec-autofill
  if (rfh->IsCredentialless())
    return;
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab_helper = ChromePasswordManagerClient::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->password_generation_driver_receivers_.Bind(rfh,
                                                         std::move(receiver));
}

ChromePasswordManagerClient::~ChromePasswordManagerClient() = default;

bool ChromePasswordManagerClient::IsSavingAndFillingEnabled(
    const GURL& url) const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    // Disable the password saving UI for automated tests. It obscures the
    // page, and there is no API to access (or dismiss) UI bubbles/infobars.
    return false;
  }
  PasswordManagerSettingsService* settings_service =
      PasswordManagerSettingsServiceFactory::GetForProfile(profile_);
  return settings_service->IsSettingEnabled(
             PasswordManagerSetting::kOfferToSavePasswords) &&
         !IsIncognito() && IsFillingEnabled(url);
}

bool ChromePasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  // Guest profiles don't have PasswordStore at all, so filling should be
  // disabled for them.
  if (!profile || profile->IsGuestSession()) {
    return false;
  }

  const bool ssl_errors = net::IsCertStatusError(GetMainFrameCertStatus());
  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogBoolean(Logger::STRING_SSL_ERRORS_PRESENT, ssl_errors);
  }
  return !ssl_errors && IsPasswordManagementEnabledForCurrentPage(url);
}

bool ChromePasswordManagerClient::IsAutoSignInEnabled() const {
  PasswordManagerSettingsService* settings_service =
      PasswordManagerSettingsServiceFactory::GetForProfile(profile_);
  return settings_service->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn);
}

bool ChromePasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  // The save password infobar and the password bubble prompt in case of
  // "webby" URLs and do not prompt in case of "non-webby" URLS (e.g. file://).
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return false;

#if BUILDFLAG(IS_ANDROID)
  if (form_to_save->IsBlocklisted())
    return false;

  save_update_password_message_delegate_.DisplaySaveUpdatePasswordPrompt(
      web_contents(), std::move(form_to_save), update_password);
#else
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  if (update_password) {
    manage_passwords_ui_controller->OnUpdatePasswordSubmitted(
        std::move(form_to_save));
  } else {
    manage_passwords_ui_controller->OnPasswordSubmitted(
        std::move(form_to_save));
  }
#endif
  return true;
}

void ChromePasswordManagerClient::PromptUserToMovePasswordToAccount(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move) {
#if !BUILDFLAG(IS_ANDROID)
  PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnShowMoveToAccountBubble(std::move(form_to_move));
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
#if !BUILDFLAG(IS_ANDROID)
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return;

  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  // There may be no UI controller for ChromeOS login page
  // (see crbug.com/774676).
  if (manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnShowManualFallbackForSaving(
        std::move(form_to_save), has_generated_password, is_update);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::HideManualFallbackForSaving() {
#if !BUILDFLAG(IS_ANDROID)
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return;

  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  // There may be no UI controller for ChromeOS login page
  // (see crbug.com/774676).
  if (manage_passwords_ui_controller)
    manage_passwords_ui_controller->OnHideManualFallbackForSaving();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::FocusedInputChanged(
    PasswordManagerDriver* driver,
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {
#if BUILDFLAG(IS_ANDROID)
  ManualFillingController::GetOrCreate(web_contents())
      ->NotifyFocusedInputChanged(focused_field_id, focused_field_type);
  password_manager::ContentPasswordManagerDriver* content_driver =
      static_cast<password_manager::ContentPasswordManagerDriver*>(driver);
  if (!PasswordAccessoryControllerImpl::ShouldAcceptFocusEvent(
          web_contents(), content_driver, focused_field_type)) {
    return;
  }

  if (!content_driver->CanShowAutofillUi())
    return;

  if (web_contents()->GetFocusedFrame()) {
    bool manual_generation_available =
        password_manager_util::ManualPasswordGenerationEnabled(driver);
    if (base::FeatureList::IsEnabled(
            password_manager::features::kUnifiedPasswordManagerErrorMessages)) {
      manual_generation_available =
          manual_generation_available &&
          password_manager_.HaveFormManagersReceivedData(driver);
    }
    GetOrCreatePasswordAccessory()->RefreshSuggestionsForField(
        focused_field_type, manual_generation_available);
  }

  PasswordGenerationController::GetOrCreate(web_contents())
      ->FocusedInputChanged(focused_field_type,
                            base::AsWeakPtr(content_driver));
#endif  // BUILDFLAG(IS_ANDROID)
}

bool ChromePasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<PasswordForm>> local_forms,
    const url::Origin& origin,
    CredentialsCallback callback) {
  // Set up an intercept callback if the prompt is zero-clickable (e.g. just one
  // form provided).
  CredentialsCallback intercept = base::BindOnce(
      &PasswordManagerClientHelper::OnCredentialsChosen,
      base::Unretained(&helper_), std::move(callback), local_forms.size() == 1);
#if BUILDFLAG(IS_ANDROID)
  // Deletes itself on the event from Java counterpart, when user interacts with
  // dialog.
  AccountChooserDialogAndroid* acccount_chooser_dialog =
      new AccountChooserDialogAndroid(web_contents(), /*client=*/this,
                                      std::move(local_forms), origin,
                                      std::move(intercept));
  return acccount_chooser_dialog->ShowDialog();
#else
  return PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnChooseCredentials(std::move(local_forms), origin,
                            std::move(intercept));
#endif
}

#if BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::ShowPasswordManagerErrorMessage(
    password_manager::ErrorMessageFlowType flow_type,
    password_manager::PasswordStoreBackendErrorType error_type) {
  if (!password_manager_error_message_delegate_) {
    password_manager_error_message_delegate_ =
        std::make_unique<PasswordManagerErrorMessageDelegate>(
            std::make_unique<PasswordManagerErrorMessageHelperBridgeImpl>());
    password_manager_error_message_delegate_->MaybeDisplayErrorMessage(
        web_contents(), GetPrefs(), flow_type, error_type,
        base::BindOnce(&ChromePasswordManagerClient::ResetErrorMessageDelegate,
                       base::Unretained(this)));
  }
}

void ChromePasswordManagerClient::ShowTouchToFill(
    PasswordManagerDriver* driver,
    autofill::mojom::SubmissionReadinessState submission_readiness) {
  auto* webauthn_delegate = GetWebAuthnCredentialsDelegateForDriver(driver);
  std::vector<password_manager::PasskeyCredential> passkeys;
  if (webauthn_delegate && webauthn_delegate->GetPasskeys().has_value()) {
    passkeys = *webauthn_delegate->GetPasskeys();
  }
  GetOrCreateTouchToFillController()->Show(
      credential_cache_
          .GetCredentialStore(url::Origin::Create(
              driver->GetLastCommittedURL().DeprecatedGetOriginAsURL()))
          .GetCredentials(),
      passkeys,
      std::make_unique<TouchToFillControllerAutofillDelegate>(
          this, GetDeviceAuthenticator(), driver->AsWeakPtr(),
          submission_readiness));
}

void ChromePasswordManagerClient::OnPasswordSelected(
    const std::u16string& text) {
  password_reuse_detection_manager_.OnPaste(text);
}
#endif

scoped_refptr<device_reauth::DeviceAuthenticator>
ChromePasswordManagerClient::GetDeviceAuthenticator() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  return ChromeDeviceAuthenticatorFactory::GetDeviceAuthenticator();
#else
  return nullptr;
#endif
}

void ChromePasswordManagerClient::GeneratePassword(
    PasswordGenerationType type) {
#if BUILDFLAG(IS_ANDROID)
  PasswordGenerationController* generation_controller =
      PasswordGenerationController::GetIfExisting(web_contents());
  base::WeakPtr<PasswordManagerDriver> driver =
      generation_controller->GetActiveFrameDriver();
  if (!driver)
    return;
  password_manager::ContentPasswordManagerDriver* content_driver =
      static_cast<password_manager::ContentPasswordManagerDriver*>(
          driver.get());
#else
  password_manager::ContentPasswordManagerDriver* content_driver =
      driver_factory_->GetDriverForFrame(web_contents()->GetFocusedFrame());
  if (!content_driver)
    return;
#endif
  // Using unretained pointer is safe because |this| outlives
  // ContentPasswordManagerDriver that holds the connection.
  content_driver->GeneratePassword(base::BindOnce(
      &ChromePasswordManagerClient::GenerationResultAvailable,
      base::Unretained(this), type, base::AsWeakPtr(content_driver)));
}

void ChromePasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<PasswordForm>> local_forms,
    const url::Origin& origin) {
  DCHECK(!local_forms.empty());
  helper_.NotifyUserAutoSignin();
#if BUILDFLAG(IS_ANDROID)
  ShowAutoSigninPrompt(web_contents(), local_forms[0]->username_value);
#else
  PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnAutoSignin(std::move(local_forms), origin);
#endif
}

void ChromePasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<PasswordForm> form) {
  helper_.NotifyUserCouldBeAutoSignedIn(std::move(form));
}

void ChromePasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI>
        submitted_manager) {
  helper_.NotifySuccessfulLoginWithExistingPassword(
      std::move(submitted_manager));
}

void ChromePasswordManagerClient::NotifyStorePasswordCalled() {
  helper_.NotifyStorePasswordCalled();
  was_store_ever_called_ = true;
}

#if BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::StartSubmissionTrackingAfterTouchToFill(
    const std::u16string& filled_username) {
  username_filled_by_touch_to_fill_ =
      std::make_pair(filled_username, base::Time::Now());
}

void ChromePasswordManagerClient::NotifyOnSuccessfulLogin(
    const std::u16string& submitted_username) {
  if (!username_filled_by_touch_to_fill_)
    return;

  base::TimeDelta delta =
      base::Time::Now() - username_filled_by_touch_to_fill_->second;
  // Filter out unrelated logins.
  if (delta < base::Minutes(1) &&
      username_filled_by_touch_to_fill_->first == submitted_username) {
    UmaHistogramMediumTimes("PasswordManager.TouchToFill.TimeToSuccessfulLogin",
                            delta);
    ukm::builders::TouchToFill_TimeToSuccessfulLogin(GetUkmSourceId())
        .SetTimeToSuccessfulLogin(
            ukm::GetExponentialBucketMinForUserTiming(delta.InMilliseconds()))
        .Record(ukm::UkmRecorder::Get());

    base::UmaHistogramBoolean(
        "PasswordManager.TouchToFill.SuccessfulSubmissionWasObserved", true);
    username_filled_by_touch_to_fill_.reset();
  } else {
    ResetSubmissionTrackingAfterTouchToFill();
  }
}

void ChromePasswordManagerClient::ResetSubmissionTrackingAfterTouchToFill() {
  if (username_filled_by_touch_to_fill_.has_value()) {
    base::UmaHistogramBoolean(
        "PasswordManager.TouchToFill.SuccessfulSubmissionWasObserved", false);
    username_filled_by_touch_to_fill_.reset();
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

void ChromePasswordManagerClient::UpdateCredentialCache(
    const url::Origin& origin,
    const std::vector<const PasswordForm*>& best_matches,
    bool is_blocklisted) {
#if BUILDFLAG(IS_ANDROID)
  credential_cache_.SaveCredentialsAndBlocklistedForOrigin(
      best_matches, CredentialCache::IsOriginBlocklisted(is_blocklisted),
      origin);

#endif
}

void ChromePasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form) {
#if BUILDFLAG(IS_ANDROID)
  generated_password_saved_message_delegate_.ShowPrompt(web_contents(),
                                                        std::move(saved_form));
#else
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnAutomaticPasswordSave(
      std::move(saved_form));
#endif
}

void ChromePasswordManagerClient::PasswordWasAutofilled(
    const std::vector<const PasswordForm*>& best_matches,
    const url::Origin& origin,
    const std::vector<const PasswordForm*>* federated_matches,
    bool was_autofilled_on_pageload) {
#if !BUILDFLAG(IS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnPasswordAutofilled(best_matches, origin,
                                                       federated_matches);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (was_autofilled_on_pageload &&
      password_manager_util::
          ShouldShowBiometricAuthenticationBeforeFillingPromo(this)) {
    PasswordsClientUIDelegateFromWebContents(web_contents())
        ->OnBiometricAuthenticationForFilling(GetPrefs());
  }
#endif
}

void ChromePasswordManagerClient::AutofillHttpAuth(
    const PasswordForm& preferred_match,
    const password_manager::PasswordFormManagerForUI* form_manager) {
  httpauth_manager_.Autofill(preferred_match, form_manager);
  DCHECK(!form_manager->GetBestMatches().empty());
  PasswordWasAutofilled(form_manager->GetBestMatches(),
                        url::Origin::Create(form_manager->GetURL()), nullptr,
                        /*was_autofilled_on_pageload=*/false);
}

void ChromePasswordManagerClient::NotifyUserCredentialsWereLeaked(
    password_manager::CredentialLeakType leak_type,
    const GURL& url,
    const std::u16string& username) {
#if BUILDFLAG(IS_ANDROID)
  auto metrics_recorder = std::make_unique<
      password_manager::metrics_util::LeakDialogMetricsRecorder>(
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
      password_manager::GetLeakDialogType(leak_type));
  (new CredentialLeakControllerAndroid(
       leak_type, url, username, web_contents()->GetTopLevelNativeWindow(),
       std::move(metrics_recorder)))
      ->ShowDialog();
#else   // !BUILDFLAG(IS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnCredentialLeak(leak_type, url, username);
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::TriggerReauthForPrimaryAccount(
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  account_storage_auth_helper_.TriggerOptInReauth(access_point,
                                                  std::move(reauth_callback));
#else
  std::move(reauth_callback).Run(ReauthSucceeded(false));
#endif
}

void ChromePasswordManagerClient::TriggerSignIn(
    signin_metrics::AccessPoint access_point) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  account_storage_auth_helper_.TriggerSignIn(access_point);
#endif
}

PrefService* ChromePasswordManagerClient::GetPrefs() const {
  return profile_->GetPrefs();
}

PrefService* ChromePasswordManagerClient::GetLocalStatePrefs() const {
  return g_browser_process->local_state();
}

const syncer::SyncService* ChromePasswordManagerClient::GetSyncService() const {
  return GetSyncServiceForProfile(profile_);
}

password_manager::PasswordStoreInterface*
ChromePasswordManagerClient::GetProfilePasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  return PasswordStoreFactory::GetForProfile(profile_,
                                             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStoreInterface*
ChromePasswordManagerClient::GetAccountPasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  return AccountPasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordReuseManager*
ChromePasswordManagerClient::GetPasswordReuseManager() const {
  return PasswordReuseManagerFactory::GetForProfile(profile_);
}

password_manager::PasswordChangeSuccessTracker*
ChromePasswordManagerClient::GetPasswordChangeSuccessTracker() {
  return password_manager::PasswordChangeSuccessTrackerFactory::
      GetForBrowserContext(profile_);
}

password_manager::SyncState ChromePasswordManagerClient::GetPasswordSyncState()
    const {
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  return password_manager_util::GetPasswordSyncState(sync_service);
}

bool ChromePasswordManagerClient::WasLastNavigationHTTPError() const {
  DCHECK(web_contents());

  std::unique_ptr<password_manager::BrowserSavePasswordProgressLogger> logger;
  if (log_manager_->IsLoggingActive()) {
    logger =
        std::make_unique<password_manager::BrowserSavePasswordProgressLogger>(
            log_manager_.get());
    logger->LogMessage(Logger::STRING_WAS_LAST_NAVIGATION_HTTP_ERROR_METHOD);
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

void ChromePasswordManagerClient::PromptUserToEnableAutosignin() {
#if BUILDFLAG(IS_ANDROID)
  // Dialog is deleted by the Java counterpart after user interacts with it.
  AutoSigninFirstRunDialogAndroid* auto_signin_first_run_dialog =
      new AutoSigninFirstRunDialogAndroid(web_contents());
  auto_signin_first_run_dialog->ShowDialog();
#else
  PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnPromptEnableAutoSignin();
#endif
}

// TODO(https://crbug.com/1225171): If off-the-record Guest is not deprecated,
// rename this function to IsOffTheRecord for better readability.
bool ChromePasswordManagerClient::IsIncognito() const {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

profile_metrics::BrowserProfileType
ChromePasswordManagerClient::GetProfileType() const {
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  return profile_metrics::GetBrowserProfileType(browser_context);
}

const password_manager::PasswordManager*
ChromePasswordManagerClient::GetPasswordManager() const {
  return &password_manager_;
}

const password_manager::PasswordFeatureManager*
ChromePasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

password_manager::HttpAuthManager*
ChromePasswordManagerClient::GetHttpAuthManager() {
  return &httpauth_manager_;
}

autofill::AutofillDownloadManager*
ChromePasswordManagerClient::GetAutofillDownloadManager() {
  autofill::ContentAutofillDriverFactory* factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents());
  if (factory) {
    autofill::ContentAutofillDriver* driver =
        factory->DriverForFrame(web_contents()->GetPrimaryMainFrame());
    // |driver| can be NULL if the tab is being closed.
    if (driver) {
      autofill::AutofillManager* autofill_manager = driver->autofill_manager();
      if (autofill_manager)
        return autofill_manager->download_manager();
    }
  }
  return nullptr;
}

bool ChromePasswordManagerClient::IsCommittedMainFrameSecure() const {
  return network::IsOriginPotentiallyTrustworthy(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

const GURL& ChromePasswordManagerClient::GetLastCommittedURL() const {
  return web_contents()->GetLastCommittedURL();
}

url::Origin ChromePasswordManagerClient::GetLastCommittedOrigin() const {
  DCHECK(web_contents());
  return web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
}
const password_manager::CredentialsFilter*
ChromePasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

autofill::LogManager* ChromePasswordManagerClient::GetLogManager() {
  return log_manager_.get();
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
      sessions::GetPasswordStateFromNavigation(entry);

  SerializedNavigationEntry::PasswordState new_state =
      (has_password_field ? SerializedNavigationEntry::HAS_PASSWORD_FIELD
                          : SerializedNavigationEntry::NO_PASSWORD_FIELD);

  if (new_state > old_state) {
    SetPasswordStateInNavigation(new_state, entry);
    if (HistoryTabHelper* history_tab_helper =
            HistoryTabHelper::FromWebContents(web_contents())) {
      history_tab_helper->OnPasswordStateUpdated(new_state);
    }
  }
}

autofill::LanguageCode ChromePasswordManagerClient::GetPageLanguage() const {
  // TODO(crbug.com/912597): iOS vs other platforms extracts language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents());
  if (translate_manager)
    return autofill::LanguageCode(
        translate_manager->GetLanguageState()->source_language());
  return autofill::LanguageCode();
}

safe_browsing::PasswordProtectionService*
ChromePasswordManagerClient::GetPasswordProtectionService() const {
  return safe_browsing::ChromePasswordProtectionService::
      GetPasswordProtectionService(profile_);
}

#if defined(ON_FOCUS_PING_ENABLED)
void ChromePasswordManagerClient::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (pps) {
    pps->MaybeStartPasswordFieldOnFocusRequest(
        web_contents(), web_contents()->GetLastCommittedURL(), form_action,
        frame_url, pps->GetAccountInfo().hosted_domain);
  }
}
#endif  // defined(ON_FOCUS_PING_ENABLED)

void ChromePasswordManagerClient::CheckProtectedPasswordEntry(
    PasswordType password_type,
    const std::string& username,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    bool password_field_exists,
    uint64_t reused_password_hash,
    const std::string& domain) {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (!pps)
    return;

  pps->MaybeStartProtectedPasswordEntryRequest(
      web_contents(), web_contents()->GetLastCommittedURL(), username,
      password_type, matching_reused_credentials, password_field_exists);

#if !BUILDFLAG(IS_ANDROID)
  // If the webpage is not an extension page, do nothing.
  if (!GURL(domain).SchemeIs(kExtensionScheme)) {
    return;
  }
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (!telemetry_service || !telemetry_service->enabled() ||
      !base::FeatureList::IsEnabled(
          safe_browsing::kExtensionTelemetryPotentialPasswordTheft)) {
    return;
  }
  // Construct password reuse info.
  safe_browsing::PasswordReuseInfo password_reuse_info;
  password_reuse_info.matches_signin_password =
      password_type == PasswordType::PRIMARY_ACCOUNT_PASSWORD;
  password_reuse_info.matching_domains =
      GetMatchingDomains(matching_reused_credentials);
  password_reuse_info.reused_password_account_type =
      pps->GetPasswordProtectionReusedPasswordAccountType(password_type,
                                                          username);
  password_reuse_info.count = 1;
  password_reuse_info.reused_password_hash = reused_password_hash;

  // Extract the host part of an extension domain, which will be the extension
  // ID.
  std::string host = GURL(domain).host();
  auto password_reuse_signal =
      std::make_unique<safe_browsing::PasswordReuseSignal>(host,
                                                           password_reuse_info);
  telemetry_service->AddSignal(std::move(password_reuse_signal));
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::LogPasswordReuseDetectedEvent() {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (pps) {
    pps->MaybeLogPasswordReuseDetectedEvent(web_contents());
  }
}

#if !BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::MaybeReportEnterpriseLoginEvent(
    const GURL& url,
    bool is_federated,
    const url::Origin& federated_origin,
    const std::u16string& login_user_name) const {
  if (!base::FeatureList::IsEnabled(policy::features::kLoginEventReporting))
    return;

  extensions::SafeBrowsingPrivateEventRouter* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
          profile_);
  if (!router)
    return;

  // The router is responsible for checking if the reporting of this event type
  // is enabled by the admin.
  router->OnLoginEvent(url, is_federated, federated_origin, login_user_name);
}

void ChromePasswordManagerClient::MaybeReportEnterprisePasswordBreachEvent(
    const std::vector<std::pair<GURL, std::u16string>>& identities) const {
  if (!base::FeatureList::IsEnabled(
          policy::features::kPasswordBreachEventReporting)) {
    return;
  }

  extensions::SafeBrowsingPrivateEventRouter* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
          profile_);
  if (!router)
    return;

  // The router is responsible for checking if the reporting of this event type
  // is enabled by the admin.
  router->OnPasswordBreach(kPasswordBreachEntryTrigger, identities);
}
#endif

ukm::SourceId ChromePasswordManagerClient::GetUkmSourceId() {
  return web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

PasswordManagerMetricsRecorder*
ChromePasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(GetUkmSourceId());
  }
  return base::OptionalToPtr(metrics_recorder_);
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

signin::IdentityManager* ChromePasswordManagerClient::GetIdentityManager() {
  return IdentityManagerFactory::GetForProfile(profile_->GetOriginalProfile());
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromePasswordManagerClient::GetURLLoaderFactory() {
  return profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

network::mojom::NetworkContext* ChromePasswordManagerClient::GetNetworkContext()
    const {
  return profile_->GetDefaultStoragePartition()->GetNetworkContext();
}

void ChromePasswordManagerClient::UpdateFormManagers() {
  password_manager_.UpdateFormManagers();
}

void ChromePasswordManagerClient::NavigateToManagePasswordsPage(
    password_manager::ManagePasswordsReferrer referrer) {
#if BUILDFLAG(IS_ANDROID)
  password_manager_launcher::ShowPasswordSettings(web_contents(), referrer,
                                                  /*manage_passkeys=*/false);
#else
  ::NavigateToManagePasswordsPage(
      chrome::FindBrowserWithWebContents(web_contents()), referrer);
#endif
}

#if BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::NavigateToManagePasskeysPage(
    password_manager::ManagePasswordsReferrer referrer) {
  password_manager_launcher::ShowPasswordSettings(web_contents(), referrer,
                                                  /*manage_passkeys=*/true);
}
#endif

bool ChromePasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  // TODO(crbug.com/862989): Move the following function (and the feature) to
  // the password component. Then remove IsIsolationForPasswordsSitesEnabled()
  // from the PasswordManagerClient interface.
  return site_isolation::SiteIsolationPolicy::
      IsIsolationForPasswordSitesEnabled();
}

bool ChromePasswordManagerClient::IsNewTabPage() const {
  auto origin = GetLastCommittedURL().DeprecatedGetOriginAsURL();
  return origin ==
             GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL() ||
         origin == GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL();
}

FieldInfoManager* ChromePasswordManagerClient::GetFieldInfoManager() const {
  return FieldInfoManagerFactory::GetForBrowserContext(profile_);
}

password_manager::WebAuthnCredentialsDelegate*
ChromePasswordManagerClient::GetWebAuthnCredentialsDelegateForDriver(
    PasswordManagerDriver* driver) {
  auto* frame_host =
      static_cast<password_manager::ContentPasswordManagerDriver*>(driver)
          ->render_frame_host();
  return ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents())
      ->GetDelegateForFrame(frame_host);
}

version_info::Channel ChromePasswordManagerClient::GetChannel() const {
  return chrome::GetChannel();
}

void ChromePasswordManagerClient::RefreshPasswordManagerSettingsIfNeeded()
    const {
#if BUILDFLAG(IS_ANDROID)
  // Settings need to be requested for android clients enrolled into the unified
  // password manager experiment.
  if (!password_manager::features::UsesUnifiedPasswordManagerUi())
    return;
  PasswordManagerSettingsServiceFactory::GetForProfile(profile_)
      ->RequestSettingsFromBackend();
#endif
}

void ChromePasswordManagerClient::AutomaticGenerationAvailable(
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          rfh, ui_data.form_data.url,
          BadMessageReason::
              CPMD_BAD_ORIGIN_AUTOMATIC_GENERATION_STATUS_CHANGED))
    return;
#if BUILDFLAG(IS_ANDROID)
  PasswordManagerDriver* driver = driver_factory_->GetDriverForFrame(rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  // TODO(crbug.com/1294378): Remove reference to nested frames once
  // EnablePasswordManagerWithinFencedFrame is launched.
  DCHECK(driver || rfh->IsNestedWithinFencedFrame());
  if (!driver) {
    return;
  }

  PasswordGenerationController* generation_controller =
      PasswordGenerationController::GetIfExisting(web_contents());
  DCHECK(generation_controller);

  gfx::RectF element_bounds_in_screen_space = TransformToRootCoordinates(
      password_generation_driver_receivers_.GetCurrentTargetFrame(),
      ui_data.bounds);

  generation_controller->OnAutomaticGenerationAvailable(
      driver, ui_data, element_bounds_in_screen_space);
#else
  password_manager::ContentPasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  // TODO(crbug.com/1294378): Remove reference to nested frames once
  // EnablePasswordManagerWithinFencedFrame is launched.
  DCHECK(driver || rfh->IsNestedWithinFencedFrame());
  if (!driver)
    return;

  // Attempt to show the autofill dropdown UI first.
  gfx::RectF element_bounds_in_top_frame_space =
      TransformToRootCoordinates(driver->render_frame_host(), ui_data.bounds);
  if (driver->GetPasswordAutofillManager()
          ->MaybeShowPasswordSuggestionsWithGeneration(
              element_bounds_in_top_frame_space, ui_data.text_direction,
              /*show_password_suggestions=*/
              ui_data.is_generation_element_password_type)) {
    // (see crbug.com/1338105)
    if (popup_controller_) {
      popup_controller_->GeneratedPasswordRejected();
    }

    driver->SetSuggestionAvailability(
        ui_data.generation_element_id,
        autofill::mojom::AutofillState::kAutofillAvailable);
    return;
  }

  ShowPasswordGenerationPopup(PasswordGenerationType::kAutomatic, driver,
                              ui_data);
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::ShowPasswordEditingPopup(
    const gfx::RectF& bounds,
    const autofill::FormData& form_data,
    autofill::FieldRendererId field_renderer_id,
    const std::u16string& password_value) {
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  auto* driver = driver_factory_->GetDriverForFrame(rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  // TODO(crbug.com/1294378): Remove reference to nested frames once
  // EnablePasswordManagerWithinFencedFrame is launched.
  DCHECK(driver || rfh->IsNestedWithinFencedFrame());
  if (!driver)
    return;

  gfx::RectF element_bounds_in_screen_space =
      GetBoundsInScreenSpace(TransformToRootCoordinates(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          bounds));
  autofill::password_generation::PasswordGenerationUIData ui_data(
      bounds, /*max_length=*/0, /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), field_renderer_id,
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      password_manager::GetFormWithFrameAndFormMetaData(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          form_data));
  popup_controller_ = PasswordGenerationPopupControllerImpl::GetOrCreate(
      popup_controller_, element_bounds_in_screen_space, ui_data,
      driver->AsWeakPtr(), observer_, web_contents(),
      password_generation_driver_receivers_.GetCurrentTargetFrame());
  DCHECK(!password_value.empty());
  popup_controller_->UpdateGeneratedPassword(password_value);
  popup_controller_->Show(
      PasswordGenerationPopupController::kEditGeneratedPassword);
}

void ChromePasswordManagerClient::PasswordGenerationRejectedByTyping() {
  if (popup_controller_)
    popup_controller_->GeneratedPasswordRejected();
}

void ChromePasswordManagerClient::PresaveGeneratedPassword(
    const autofill::FormData& form_data,
    const std::u16string& password_value) {
  if (popup_controller_)
    popup_controller_->UpdateGeneratedPassword(password_value);

  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  PasswordManagerDriver* driver = driver_factory_->GetDriverForFrame(rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  // TODO(crbug.com/1294378): Remove reference to nested frames once
  // EnablePasswordManagerWithinFencedFrame is launched.
  DCHECK(driver || rfh->IsNestedWithinFencedFrame());
  if (driver) {
    password_manager_.OnPresaveGeneratedPassword(
        driver,
        password_manager::GetFormWithFrameAndFormMetaData(
            password_generation_driver_receivers_.GetCurrentTargetFrame(),
            form_data),
        password_value);
  }
}

void ChromePasswordManagerClient::PasswordNoLongerGenerated(
    const autofill::FormData& form_data) {
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  PasswordManagerDriver* driver = driver_factory_->GetDriverForFrame(rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  // TODO(crbug.com/1294378): Remove reference to nested frames once
  // EnablePasswordManagerWithinFencedFrame is launched.
  DCHECK(driver || rfh->IsNestedWithinFencedFrame());
  if (!driver)
    return;
  password_manager_.OnPasswordNoLongerGenerated(
      driver, password_manager::GetFormWithFrameAndFormMetaData(
                  password_generation_driver_receivers_.GetCurrentTargetFrame(),
                  form_data));

  PasswordGenerationPopupController* controller = popup_controller_.get();
  if (controller &&
      controller->state() ==
          PasswordGenerationPopupController::kEditGeneratedPassword) {
    popup_controller_->GeneratedPasswordRejected();
  }
}

void ChromePasswordManagerClient::FrameWasScrolled() {
  if (popup_controller_)
    popup_controller_->FrameWasScrolled();
}

void ChromePasswordManagerClient::GenerationElementLostFocus() {
  // TODO(crbug.com/968046): Look into removing this since FocusedInputChanged
  // seems to be a good replacement.
  if (popup_controller_)
    popup_controller_->GenerationElementLostFocus();
}

#if BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::OnImeTextCommittedEvent(
    const std::u16string& text_str) {
  password_reuse_detection_manager_.OnKeyPressedCommitted(text_str);
}

void ChromePasswordManagerClient::OnImeSetComposingTextEvent(
    const std::u16string& text_str) {
  last_composing_text_ = text_str;
  password_reuse_detection_manager_.OnKeyPressedUncommitted(
      last_composing_text_);
}

void ChromePasswordManagerClient::OnImeFinishComposingTextEvent() {
  password_reuse_detection_manager_.OnKeyPressedCommitted(last_composing_text_);
  last_composing_text_.clear();
}
#endif  // BUILDFLAG(IS_ANDROID)

void ChromePasswordManagerClient::SetTestObserver(
    PasswordGenerationPopupObserver* observer) {
  observer_ = observer;
}

// static
void ChromePasswordManagerClient::BindCredentialManager(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CredentialManager> receiver) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  // Only valid for the currently committed RenderFrameHost, and not, e.g. old
  // zombie RFH's being swapped out following cross-origin navigations.
  if (web_contents->GetPrimaryMainFrame() != render_frame_host)
    return;

  // The ChromePasswordManagerClient will not bind the mojo interface for
  // non-primary frames, e.g. BackForwardCache, Prerenderer, since the
  // MojoBinderPolicy prevents this interface from being granted.
  DCHECK_EQ(render_frame_host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  ChromePasswordManagerClient* instance =
      ChromePasswordManagerClient::FromWebContents(web_contents);

  // Try to bind to the driver, but if driver is not available for this render
  // frame host, the request will be just dropped. This will cause the message
  // pipe to be closed, which will raise a connection error on the peer side.
  if (!instance)
    return;

  // Disable BackForwardCache for this page.
  // This is necessary because ContentCredentialManager::DisconnectBinding()
  // will be called when the page is navigated away from, leaving it
  // in an unusable state if the page is restored from the BackForwardCache.
  //
  // It looks like in order to remove this workaround, we probably just need to
  // make the CredentialManager mojo API rebind on the renderer side when the
  // next call is made, if it has become disconnected.
  // TODO(https://crbug.com/1015358): Remove this workaround.
  content::BackForwardCache::DisableForRenderFrameHost(
      render_frame_host,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::
              kChromePasswordManagerClient_BindCredentialManager));

  instance->content_credential_manager_.BindRequest(std::move(receiver));
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

#if BUILDFLAG(IS_ANDROID)
PasswordAccessoryController*
ChromePasswordManagerClient::GetOrCreatePasswordAccessory() {
  return PasswordAccessoryController::GetOrCreate(web_contents(),
                                                  &credential_cache_);
}

TouchToFillController*
ChromePasswordManagerClient::GetOrCreateTouchToFillController() {
  if (!touch_to_fill_controller_) {
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>();
  }
  return touch_to_fill_controller_.get();
}
#endif  // BUILDFLAG(IS_ANDROID)

ChromePasswordManagerClient::ChromePasswordManagerClient(
    content::WebContents* web_contents,
    autofill::AutofillClient* autofill_client)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromePasswordManagerClient>(*web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      password_manager_(this),
      password_feature_manager_(profile_->GetPrefs(),
                                g_browser_process->local_state(),
                                SyncServiceFactory::GetForProfile(profile_)),
      httpauth_manager_(this),
      password_reuse_detection_manager_(this),
      driver_factory_(nullptr),
      content_credential_manager_(this),
      password_generation_driver_receivers_(web_contents, this),
      observer_(nullptr),
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      credentials_filter_(
          this,
          base::BindRepeating(&GetSyncServiceForProfile, profile_),
          DiceWebSigninInterceptorFactory::GetForProfile(profile_)),
      account_storage_auth_helper_(
          IdentityManagerFactory::GetForProfile(profile_),
          &password_feature_manager_,
          base::BindRepeating(
              [](content::WebContents* web_contents) {
                Browser* browser =
                    chrome::FindBrowserWithWebContents(web_contents);
                return browser ? browser->signin_view_controller() : nullptr;
              },
              web_contents)),
#else
      credentials_filter_(
          this,
          base::BindRepeating(&GetSyncServiceForProfile, profile_)),
#endif
      helper_(this) {
  ContentPasswordManagerDriverFactory::CreateForWebContents(web_contents, this,
                                                            autofill_client);
  driver_factory_ =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents);
  log_manager_ = autofill::LogManager::Create(
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          profile_),
      base::BindRepeating(
          &ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability,
          base::Unretained(driver_factory_)));

  driver_factory_->RequestSendLoggingAvailability();
}

void ChromePasswordManagerClient::PrimaryPageChanged(content::Page& page) {
  // Logging has no sense on WebUI sites.
  log_manager_->SetSuspended(web_contents()->GetWebUI() != nullptr);

  // Send any collected metrics by destroying the metrics recorder.
  metrics_recorder_.reset();

  httpauth_manager_.OnDidFinishMainFrameNavigation();

  // From this point on, the ContentCredentialManager will service API calls in
  // the context of the new WebContents::GetLastCommittedURL, which may very
  // well be cross-origin. Disconnect existing client, and drop pending
  // requests.
  content_credential_manager_.DisconnectBinding();

  password_reuse_detection_manager_.DidNavigateMainFrame(GetLastCommittedURL());

  AddToWidgetInputEventObservers(page.GetMainDocument().GetRenderWidgetHost(),
                                 this);

#if BUILDFLAG(IS_ANDROID)
  // This unblocklisted info is only used after form submission to determine
  // whether to record PasswordManager.SaveUIDismissalReasonAfterUnblacklisting.
  // Therefore it is sufficient to save it only once on navigation and not
  // every time the user changes the UI toggle.
  password_manager_.MarkWasUnblocklistedInFormManagers(&credential_cache_);
  credential_cache_.ClearCredentials();
#endif  // BUILDFLAG(IS_ANDROID)

  // Hide form filling UI on navigating away.
  HideFillingUI();
}

void ChromePasswordManagerClient::WebContentsDestroyed() {
  // crbug/1090011
  // Drop the connection before the WebContentsObserver destructors are invoked.
  // Other classes may contain callbacks to the Mojo methods. Those callbacks
  // don't like to be destroyed earlier than the pipe itself.
  content_credential_manager_.DisconnectBinding();

#if BUILDFLAG(IS_ANDROID)
  save_update_password_message_delegate_.DismissSaveUpdatePasswordPrompt();
  if (password_manager_error_message_delegate_) {
    password_manager_error_message_delegate_
        ->DismissPasswordManagerErrorMessage(
            messages::DismissReason::TAB_DESTROYED);
  }
#endif
}

void ChromePasswordManagerClient::OnPaste() {
  std::u16string text;
  bool used_crosapi_workaround = false;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, the ozone/wayland clipboard implementation is asynchronous by
  // default and runs a nested message loop to fake synchroncity. This in turn
  // causes crashes. See https://crbug.com/1155662 for details. In the short
  // term, we skip ozone/wayland entirely and use a synchronous crosapi to get
  // clipboard text.
  // TODO(https://crbug.com/913422): This logic can be removed once all
  // clipboard APIs are async.
  auto* service = chromeos::LacrosService::Get();
  if (service->IsAvailable<crosapi::mojom::Clipboard>()) {
    used_crosapi_workaround = true;
    std::string text_utf8;
    {
      crosapi::ScopedAllowSyncCall allow_sync_call;
      service->GetRemote<crosapi::mojom::Clipboard>()->GetCopyPasteText(
          &text_utf8);
    }
    text = base::UTF8ToUTF16(text_utf8);
  }
#endif

  if (!used_crosapi_workaround) {
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    // Given that this clipboard data read happens in the background and not
    // initiated by a user gesture, then the user shouldn't see a notification
    // if the clipboard is restricted by the rules of data leak prevention
    // policy.
    ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
        ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, &data_dst, &text);
  }

  was_on_paste_called_ = true;
  password_reuse_detection_manager_.OnPaste(std::move(text));
}

void ChromePasswordManagerClient::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // TODO(https://crbug.com/1315689): In context of Phishguard, we should handle
  // input events on subframes separately, so that we can accurately report that
  // the password was reused on a subframe. Currently any password reuse for
  // this WebContents will report password reuse on the main frame URL.
  AddToWidgetInputEventObservers(render_frame_host->GetRenderWidgetHost(),
                                 this);
}

void ChromePasswordManagerClient::OnInputEvent(
    const blink::WebInputEvent& event) {
#if BUILDFLAG(IS_ANDROID)

  // On Android, key down events are triggered if a user types in through a
  // number bar on Android keyboard. If text is typed in through other parts of
  // Android keyboard, ImeTextCommittedEvent is triggered instead.
  if (event.GetType() != blink::WebInputEvent::Type::kKeyDown)
    return;
  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);
  password_reuse_detection_manager_.OnKeyPressedCommitted(key_event.text);

#else   // !BUILDFLAG(IS_ANDROID)
  if (event.GetType() != blink::WebInputEvent::Type::kChar)
    return;
  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);
  // Key & 0x1f corresponds to the value of the key when either the control or
  // command key is pressed. This detects CTRL+V, COMMAND+V, and CTRL+SHIFT+V.
  if (key_event.windows_key_code == (ui::VKEY_V & 0x1f)) {
    OnPaste();
  } else {
    password_reuse_detection_manager_.OnKeyPressedCommitted(key_event.text);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

gfx::RectF ChromePasswordManagerClient::GetBoundsInScreenSpace(
    const gfx::RectF& bounds) {
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  return bounds + client_area.OffsetFromOrigin();
}

void ChromePasswordManagerClient::HideFillingUI() {
#if BUILDFLAG(IS_ANDROID)
  base::WeakPtr<ManualFillingController> mf_controller =
      ManualFillingController::Get(web_contents());
  // Hides all the manual filling UI if the controller already exists.
  if (mf_controller)
    mf_controller->Hide();
#endif  // BUILDFLAG(IS_ANDROID)
}

bool ChromePasswordManagerClient::IsPasswordManagementEnabledForCurrentPage(
    const GURL& url) const {
  bool is_enabled = CanShowBubbleOnURL(url);

  // The password manager is disabled on Google Password Manager page.
  if (url.DeprecatedGetOriginAsURL() ==
      GURL(password_manager::kPasswordManagerAccountDashboardURL)) {
    is_enabled = false;
  }

  // SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing
  // warnings until user interaction. If the current page has a delayed warning,
  // it'll have a user interaction observer attached. Disable password
  // management in that case.
  if (auto* observer =
          safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
              web_contents())) {
    observer->OnPasswordSaveOrAutofillDenied();
    is_enabled = false;
  }

  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogURL(Logger::STRING_SECURITY_ORIGIN, url);
    logger.LogBoolean(
        Logger::STRING_PASSWORD_MANAGEMENT_ENABLED_FOR_CURRENT_PAGE,
        is_enabled);
  }
  return is_enabled;
}

// static
bool ChromePasswordManagerClient::ShouldAnnotateNavigationEntries(
    Profile* profile) {
  // Only annotate PasswordState onto the navigation entry if user is
  // opted into UMA and they're not syncing w/ a custom passphrase.
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled())
    return false;

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service || !sync_service->IsSyncFeatureActive() ||
      sync_service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    return false;
  }

  return true;
}

void ChromePasswordManagerClient::GenerationResultAvailable(
    PasswordGenerationType type,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
    const absl::optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data) {
  if (!ui_data || !driver)
    return;
  // Check the data because it's a Mojo callback and the input isn't trusted.
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          driver->render_frame_host(), ui_data->form_data.url,
          BadMessageReason::
              CPMD_BAD_ORIGIN_SHOW_MANUAL_PASSWORD_GENERATION_POPUP))
    return;
#if BUILDFLAG(IS_ANDROID)
  PasswordGenerationController* password_generation_controller =
      PasswordGenerationController::GetIfExisting(web_contents());
  DCHECK(password_generation_controller);

  password_generation_controller->ShowManualGenerationDialog(driver.get(),
                                                             ui_data.value());
#else
  ShowPasswordGenerationPopup(type, driver.get(), *ui_data);
#endif
}

void ChromePasswordManagerClient::ShowPasswordGenerationPopup(
    PasswordGenerationType type,
    password_manager::ContentPasswordManagerDriver* driver,
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  gfx::RectF element_bounds_in_top_frame_space =
      TransformToRootCoordinates(driver->render_frame_host(), ui_data.bounds);

  gfx::RectF element_bounds_in_screen_space =
      GetBoundsInScreenSpace(element_bounds_in_top_frame_space);
  password_manager_.SetGenerationElementAndTypeForForm(
      driver, ui_data.form_data.unique_renderer_id,
      ui_data.generation_element_id, type);

  popup_controller_ = PasswordGenerationPopupControllerImpl::GetOrCreate(
      popup_controller_, element_bounds_in_screen_space, ui_data,
      driver->AsWeakPtr(), observer_, web_contents(),
      driver->render_frame_host());
  popup_controller_->UpdateTypedPassword(ui_data.user_typed_password);

  // TODO(crbug.com/1345766): Add separate flag for calculating strength and use
  // this one only when UI needs to be displayed.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordStrengthIndicator)) {
    popup_controller_->UpdatePopupBasedOnTypedPasswordStrength();
  } else {
    popup_controller_->Show(
        PasswordGenerationPopupController::kOfferGeneration);
  }

  driver->SetSuggestionAvailability(
      ui_data.generation_element_id,
      popup_controller_ && popup_controller_->IsVisible()
          ? autofill::mojom::AutofillState::kAutofillAvailable
          : autofill::mojom::AutofillState::kNoSuggestions);
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

#if BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::ResetErrorMessageDelegate() {
  password_manager_error_message_delegate_.reset();
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromePasswordManagerClient);
