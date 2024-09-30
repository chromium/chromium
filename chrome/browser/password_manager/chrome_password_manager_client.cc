// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/android/first_cct_page_load_marker.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/password_manager/field_info_manager_factory.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/webauthn/authenticator_request_window.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/renderer_forms_with_server_predictions.h"
#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/content/browser/password_requirements_service_factory.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/translate/core/browser/translate_manager.h"
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
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller_impl.h"
#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"
#include "chrome/browser/password_manager/android/account_chooser_dialog_android.h"
#include "chrome/browser/password_manager/android/auto_signin_first_run_dialog_android.h"
#include "chrome/browser/password_manager/android/auto_signin_prompt_controller.h"
#include "chrome/browser/password_manager/android/cred_man_controller.h"
#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper_impl.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge_impl.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/password_manager/android/password_manager_ui_util_android.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "chrome/browser/password_manager/android/password_migration_warning_startup_launcher.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_autofill_delegate.h"
#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller_impl.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/password_credential_filler_impl.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"
#else
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"
#include "components/policy/core/common/features.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/browser.h"
#endif

#if BUILDFLAG(IS_ANDROID)
using base::android::BuildInfo;
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

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace {

#if !BUILDFLAG(IS_ANDROID)
static const char kPasswordBreachEntryTrigger[] = "PASSWORD_ENTRY";
#endif

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/41485955): Get rid of DeprecatedGetOriginAsURL().
url::Origin URLToOrigin(GURL url) {
  return url::Origin::Create(url.DeprecatedGetOriginAsURL());
}

void MaybeShowAccessLossWarning(
    PrefService* prefs,
    base::WeakPtr<content::WebContents> web_contents,
    Profile* profile) {
  if (!web_contents) {
    return;
  }
  PasswordAccessLossWarningBridgeImpl bridge;
  if (bridge.ShouldShowAccessLossNoticeSheet(prefs,
                                             /*called_at_startup=*/true)) {
    bridge.MaybeShowAccessLossNoticeSheet(
        prefs, web_contents->GetTopLevelNativeWindow(), profile,
        /*called_at_startup=*/true,
        password_manager_android_util::PasswordAccessLossWarningTriggers::
            kChromeStartup);
  }
}

void MaybeShowPostMigrationSheetWrapper(
    base::WeakPtr<content::WebContents> web_contents,
    Profile* profile) {
  if (!web_contents) {
    return;
  }
  local_password_migration::MaybeShowPostMigrationSheet(
      web_contents->GetTopLevelNativeWindow(), profile);
}

#endif

}  // namespace

// static
void ChromePasswordManagerClient::CreateForWebContents(
    content::WebContents* contents) {
  if (FromWebContents(contents)) {
    return;
  }

  contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new ChromePasswordManagerClient(contents)));
}

// static
void ChromePasswordManagerClient::BindPasswordGenerationDriver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordGenerationDriver>
        receiver,
    content::RenderFrameHost* rfh) {
  // [spec] https://wicg.github.io/anonymous-iframe/#spec-autofill
  if (rfh->IsCredentialless()) {
    return;
  }
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }
  auto* tab_helper = ChromePasswordManagerClient::FromWebContents(web_contents);
  if (!tab_helper) {
    return;
  }
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
  password_manager::PasswordManagerSettingsService* settings_service =
      PasswordManagerSettingsServiceFactory::GetForProfile(profile_);
  return settings_service->IsSettingEnabled(
             PasswordManagerSetting::kOfferToSavePasswords) &&
         !IsOffTheRecord() && IsFillingEnabled(url);
}

bool ChromePasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  // Guest profiles don't have PasswordStore at all, so filling should be
  // disabled for them.
  if (!profile || profile->IsGuestSession()) {
    return false;
  }

  // Filling is impossible if password store in unavailable.
  if (!GetProfilePasswordStore()) {
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
#if BUILDFLAG(IS_ANDROID)
  if (BuildInfo::GetInstance()->is_automotive()) {
    return false;
  }
#endif
  password_manager::PasswordManagerSettingsService* settings_service =
      PasswordManagerSettingsServiceFactory::GetForProfile(profile_);
  return settings_service->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn);
}

void ChromePasswordManagerClient::TriggerUserPerceptionOfPasswordManagerSurvey(
    const std::string& filling_assistance) {
#if !BUILDFLAG(IS_ANDROID)
  if (filling_assistance.empty()) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }

  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerAutofillPasswordUserPerception, web_contents(),
      /*timeout_ms=*/5000, /*product_specific_bits_data=*/
      {}, {{"Filling assistance", filling_assistance}});
#endif
}

bool ChromePasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  // The save password infobar and the password bubble prompt in case of
  // "webby" URLs and do not prompt in case of "non-webby" URLS (e.g. file://).
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL())) {
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  if (form_to_save->IsBlocklisted()) {
    if (log_manager_->IsLoggingActive()) {
      password_manager::BrowserSavePasswordProgressLogger logger(
          log_manager_.get());
      logger.LogMessage(Logger::STRING_SAVING_BLOCKLISTED_EXPLICITLY);
    }
    return false;
  }

  // base::Unretained() is safe: If the callback is called, AccountStorageNotice
  // is alive, then so are its parent ChromePasswordManagerClient, its sibling
  // SaveUpdatePasswordMessageDelegate and web_contents() (the client is per
  // web_contents()).
  MaybeShowAccountStorageNotice(base::BindOnce(
      &SaveUpdatePasswordMessageDelegate::DisplaySaveUpdatePasswordPrompt,
      base::Unretained(&save_update_password_message_delegate_),
      base::Unretained(web_contents()), std::move(form_to_save),
      update_password, base::Unretained(this)));
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
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL())) {
    return;
  }

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
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL())) {
    return;
  }

  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  // There may be no UI controller for ChromeOS login page
  // (see crbug.com/774676).
  if (manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnHideManualFallbackForSaving();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::FocusedInputChanged(
    PasswordManagerDriver* driver,
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {
#if BUILDFLAG(IS_ANDROID)
  // Suppress keyboard accessory if password store is not available.
  if (GetProfilePasswordStore() == nullptr) {
    return;
  }
  ManualFillingController::GetOrCreate(web_contents())
      ->NotifyFocusedInputChanged(focused_field_id, focused_field_type);
  GetOrCreatePasswordAccessory()->UpdateCredManReentryUi(focused_field_type);

  password_manager::ContentPasswordManagerDriver* content_driver =
      static_cast<password_manager::ContentPasswordManagerDriver*>(driver);
  if (!ShouldAcceptFocusEvent(web_contents(), content_driver,
                              focused_field_type)) {
    return;
  }

  if (!content_driver->CanShowAutofillUi()) {
    return;
  }

  if (web_contents()->GetFocusedFrame()) {
    GetOrCreatePasswordAccessory()->RefreshSuggestionsForField(
        focused_field_type);
  }

  PasswordGenerationController::GetOrCreate(web_contents())
      ->FocusedInputChanged(focused_field_type,
                            content_driver->AsWeakPtrImpl());
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
  bool oldGMSSavingDisabled = error_type ==
                              password_manager::PasswordStoreBackendErrorType::
                                  kGMSCoreOutdatedSavingDisabled;
  bool oldGMSSavingPossible = error_type ==
                              password_manager::PasswordStoreBackendErrorType::
                                  kGMSCoreOutdatedSavingPossible;
  bool noPlayStore = !password_manager_android_util::IsPlayStoreAppPresent();
  if ((oldGMSSavingDisabled || oldGMSSavingPossible) && noPlayStore) {
    return;
  }
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

void ChromePasswordManagerClient::ShowKeyboardReplacingSurface(
    password_manager::PasswordManagerDriver* driver,
    const password_manager::PasswordFillingParams& password_filling_params,
    bool is_webauthn_form,
    base::OnceCallback<void(bool)> shown_cb) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordSuggestionBottomSheetV2) &&
      keyboard_replacing_surface_visibility_controller_ &&
      !keyboard_replacing_surface_visibility_controller_->CanBeShown()) {
    std::move(shown_cb).Run(
        keyboard_replacing_surface_visibility_controller_->IsVisible());
    return;
  }

  password_manager::ContentPasswordManagerDriver* content_driver =
      static_cast<password_manager::ContentPasswordManagerDriver*>(driver);

  if (GetOrCreateCredManController()->Show(
          GetWebAuthnCredManDelegateForDriver(driver),
          std::make_unique<password_manager::PasswordCredentialFillerImpl>(
              driver->AsWeakPtr(), password_filling_params),
          content_driver->AsWeakPtrImpl(), is_webauthn_form)) {
    std::move(shown_cb).Run(true);
    return;
  }

  // base::Unretained() is safe: if the callback is called, AccountStorageNotice
  // is alive, then so is its parent ChromePasswordManagerClient.
  MaybeShowAccountStorageNotice(
      base::BindOnce(&ChromePasswordManagerClient::
                         ShowKeyboardReplacingSurfaceOnAccountStorageNoticeDone,
                     base::Unretained(this), content_driver->AsWeakPtrImpl(),
                     password_filling_params, std::move(shown_cb)));
}

void ChromePasswordManagerClient::
    ShowKeyboardReplacingSurfaceOnAccountStorageNoticeDone(
        base::WeakPtr<password_manager::ContentPasswordManagerDriver>
            weak_driver,
        const password_manager::PasswordFillingParams& password_filling_params,
        base::OnceCallback<void(bool)> shown_cb) {
  // TODO(crbug.com/346748438): Maybe don't show TTF if there was a navigation.
  if (!weak_driver) {
    return std::move(shown_cb).Run(false);
  }

  password_manager::ContentPasswordManagerDriver* driver = weak_driver.get();
  auto* webauthn_delegate = GetWebAuthnCredentialsDelegateForDriver(driver);
  std::vector<password_manager::PasskeyCredential> passkeys;
  bool should_show_hybrid_option = false;
  if (webauthn_delegate && webauthn_delegate->GetPasskeys().has_value()) {
    passkeys = *webauthn_delegate->GetPasskeys();
    should_show_hybrid_option =
        webauthn_delegate->IsSecurityKeyOrHybridFlowAvailable();
  }
  auto filler =
      std::make_unique<password_manager::PasswordCredentialFillerImpl>(
          driver->AsWeakPtr(), password_filling_params);
  const PasswordForm* form_to_fill = password_manager_.GetParsedObservedForm(
      driver, password_filling_params.focused_field_renderer_id_);
  auto ttf_controller_autofill_delegate =
      std::make_unique<TouchToFillControllerAutofillDelegate>(
          this, GetDeviceAuthenticator(), webauthn_delegate->AsWeakPtr(),
          std::move(filler), form_to_fill,
          password_filling_params.focused_field_renderer_id_,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(
              should_show_hybrid_option));

  const bool shown = GetOrCreateTouchToFillController()->Show(
      credential_cache_
          .GetCredentialStore(URLToOrigin(driver->GetLastCommittedURL()))
          .GetCredentials(),
      passkeys, std::move(ttf_controller_autofill_delegate),
      GetWebAuthnCredManDelegateForDriver(driver), driver->AsWeakPtrImpl());
  std::move(shown_cb).Run(shown);
}
#endif

bool ChromePasswordManagerClient::IsReauthBeforeFillingRequired(
    device_reauth::DeviceAuthenticator* authenticator) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (!GetLocalStatePrefs() || !GetPrefs() || !authenticator) {
    return false;
  }
  return GetPasswordFeatureManager()
      ->IsBiometricAuthenticationBeforeFillingEnabled();
#elif BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    CHECK(authenticator);
    return true;
  }
  if (!authenticator || !GetPrefs()) {
    return false;
  }
  device_reauth::BiometricStatus biometric_status =
      authenticator->GetBiometricAvailabilityStatus();
  base::UmaHistogramBoolean(
      "PasswordManager.BiometricAuthPwdFillAndroid."
      "CanAuthenticateWithBiometricOrScreenLock",
      biometric_status != device_reauth::BiometricStatus::kUnavailable);
  switch (biometric_status) {
    case device_reauth::BiometricStatus::kRequired:
      return true;
    case device_reauth::BiometricStatus::kBiometricsAvailable:
    case device_reauth::BiometricStatus::kOnlyLskfAvailable:
      return base::FeatureList::IsEnabled(
                 password_manager::features::kBiometricTouchToFill) &&
             GetPrefs()->GetBoolean(password_manager::prefs::
                                        kBiometricAuthenticationBeforeFilling);
    case device_reauth::BiometricStatus::kUnavailable:
      return false;
  }
#else
  return false;
#endif
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
ChromePasswordManagerClient::GetDeviceAuthenticator() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
  device_reauth::DeviceAuthParams params(
      base::Seconds(60), device_reauth::DeviceAuthSource::kPasswordManager);

  return ChromeDeviceAuthenticatorFactory::GetForProfile(
      profile_, web_contents()->GetTopLevelNativeWindow(), params);
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
  if (!driver) {
    return;
  }
  password_manager::ContentPasswordManagerDriver* content_driver =
      static_cast<password_manager::ContentPasswordManagerDriver*>(
          driver.get());
#else
  password_manager::ContentPasswordManagerDriver* content_driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          web_contents()->GetFocusedFrame());
  if (!content_driver) {
    return;
  }
#endif
  // Using unretained pointer is safe because |this| outlives
  // ContentPasswordManagerDriver that holds the connection.
  content_driver->GeneratePassword(base::BindOnce(
      &ChromePasswordManagerClient::GenerationResultAvailable,
      base::Unretained(this), type, content_driver->AsWeakPtrImpl()));
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
  if (!username_filled_by_touch_to_fill_) {
    return;
  }

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
    base::span<const PasswordForm> best_matches,
    bool is_blocklisted) {
#if BUILDFLAG(IS_ANDROID)
  credential_cache_.SaveCredentialsAndBlocklistedForOrigin(
      best_matches, CredentialCache::IsOriginBlocklisted(is_blocklisted),
      origin);

#endif
}

void ChromePasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form,
    bool is_update_confirmation) {
#if BUILDFLAG(IS_ANDROID)
  generated_password_saved_message_delegate_.ShowPrompt(web_contents(),
                                                        std::move(saved_form));
#else
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnAutomaticPasswordSave(
      std::move(saved_form), is_update_confirmation);
#endif
}

void ChromePasswordManagerClient::PasswordWasAutofilled(
    base::span<const PasswordForm> best_matches,
    const url::Origin& origin,
    base::span<const PasswordForm> federated_matches,
    bool was_autofilled_on_pageload) {
#if !BUILDFLAG(IS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnPasswordAutofilled(best_matches, origin,
                                                       federated_matches);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  if (was_autofilled_on_pageload &&
      !IsAuthenticatorRequestWindowUrl(GetLastCommittedURL()) &&
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
                        url::Origin::Create(form_manager->GetURL()), {},
                        /*was_autofilled_on_pageload=*/false);
}

void ChromePasswordManagerClient::NotifyUserCredentialsWereLeaked(
    password_manager::CredentialLeakType leak_type,
    const GURL& url,
    const std::u16string& username,
    bool in_account_store) {
#if BUILDFLAG(IS_ANDROID)
  auto metrics_recorder = std::make_unique<
      password_manager::metrics_util::LeakDialogMetricsRecorder>(
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
      password_manager::GetLeakDialogType(leak_type));
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  // If the leaked credential is stored in the account store, the user should be
  // able to access password check for the account from the leak detection
  // dialog that is about to be shown. If the leaked credential is stored only
  // in the local store, password check for local should be accessible from the
  // dialog.
  std::string account =
      in_account_store && password_manager::sync_util::HasChosenToSyncPasswords(
                              sync_service)
          ? sync_service->GetAccountInfo().email
          : "";
  (new CredentialLeakControllerAndroid(
       leak_type, url, username, profile_,
       web_contents()->GetTopLevelNativeWindow(),
       std::make_unique<PasswordCheckupLauncherHelperImpl>(),
       std::move(metrics_recorder), account))
      ->ShowDialog();
#else   // !BUILDFLAG(IS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnCredentialLeak(leak_type, url, username);
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::NotifyKeychainError() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnKeychainError();
#endif
}

void ChromePasswordManagerClient::TriggerReauthForPrimaryAccount(
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  account_storage_auth_helper_.TriggerOptInReauth(access_point,
                                                  std::move(reauth_callback));
#else
  std::move(reauth_callback).Run(ReauthSucceeded(false));
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

void ChromePasswordManagerClient::TriggerSignIn(
    signin_metrics::AccessPoint access_point) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
  if (SyncServiceFactory::HasSyncService(profile_)) {
    return SyncServiceFactory::GetForProfile(profile_);
  }
  return nullptr;
}

affiliations::AffiliationService*
ChromePasswordManagerClient::GetAffiliationService() {
  return AffiliationServiceFactory::GetForProfile(profile_);
}

password_manager::PasswordStoreInterface*
ChromePasswordManagerClient::GetProfilePasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsOffTheRecord
  // itself when it shouldn't access the PasswordStore.
  return ProfilePasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStoreInterface*
ChromePasswordManagerClient::GetAccountPasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsOffTheRecord
  // itself when it shouldn't access the PasswordStore.
  return AccountPasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordReuseManager*
ChromePasswordManagerClient::GetPasswordReuseManager() const {
  return PasswordReuseManagerFactory::GetForProfile(profile_);
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
  if (!entry) {
    return false;
  }
  int http_status_code = entry->GetHttpStatusCode();

  if (logger) {
    logger->LogNumber(Logger::STRING_HTTP_STATUS_CODE, http_status_code);
  }

  if (http_status_code >= 400 && http_status_code < 600) {
    return true;
  }
  return false;
}

net::CertStatus ChromePasswordManagerClient::GetMainFrameCertStatus() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    return 0;
  }
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

bool ChromePasswordManagerClient::IsOffTheRecord() const {
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

autofill::AutofillCrowdsourcingManager*
ChromePasswordManagerClient::GetAutofillCrowdsourcingManager() {
  if (auto* client =
          autofill::ContentAutofillClient::FromWebContents(web_contents())) {
    return client->GetCrowdsourcingManager();
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
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    return;
  }

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
  // TODO(crbug.com/41430413): iOS vs other platforms extracts language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents());
  if (translate_manager) {
    return autofill::LanguageCode(
        translate_manager->GetLanguageState()->source_language());
  }
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

#if !BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::MaybeReportEnterpriseLoginEvent(
    const GURL& url,
    bool is_federated,
    const url::SchemeHostPort& federated_origin,
    const std::u16string& login_user_name) const {
  extensions::SafeBrowsingPrivateEventRouter* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
          profile_);
  if (!router) {
    return;
  }

  // The router is responsible for checking if the reporting of this event type
  // is enabled by the admin.
  router->OnLoginEvent(url, is_federated, federated_origin, login_user_name);
}

void ChromePasswordManagerClient::MaybeReportEnterprisePasswordBreachEvent(
    const std::vector<std::pair<GURL, std::u16string>>& identities) const {
  extensions::SafeBrowsingPrivateEventRouter* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
          profile_);
  if (!router) {
    return;
  }

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

#if BUILDFLAG(IS_ANDROID)
password_manager::FirstCctPageLoadPasswordsUkmRecorder*
ChromePasswordManagerClient::GetFirstCctPageLoadUkmRecorder() {
  if (first_cct_page_load_metrics_recorder_) {
    return first_cct_page_load_metrics_recorder_.get();
  }
  return nullptr;
}
#endif

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

FieldInfoManager* ChromePasswordManagerClient::GetFieldInfoManager() const {
  return FieldInfoManagerFactory::GetForProfile(profile_);
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
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser) {
    browser = chrome::FindLastActive();
  }
  ::NavigateToManagePasswordsPage(browser, referrer);
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
  // TODO(crbug.com/41401202): Move the following function (and the feature) to
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

password_manager::WebAuthnCredentialsDelegate*
ChromePasswordManagerClient::GetWebAuthnCredentialsDelegateForDriver(
    PasswordManagerDriver* driver) {
  auto* frame_host =
      static_cast<password_manager::ContentPasswordManagerDriver*>(driver)
          ->render_frame_host();
  return ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents())
      ->GetDelegateForFrame(frame_host);
}

#if BUILDFLAG(IS_ANDROID)
webauthn::WebAuthnCredManDelegate*
ChromePasswordManagerClient::GetWebAuthnCredManDelegateForDriver(
    PasswordManagerDriver* driver) {
  auto* frame_host =
      static_cast<password_manager::ContentPasswordManagerDriver*>(driver)
          ->render_frame_host();
  return webauthn::WebAuthnCredManDelegateFactory::GetFactory(web_contents())
      ->GetRequestDelegate(frame_host);
}

void ChromePasswordManagerClient::MarkSharedCredentialsAsNotified(
    const GURL& url) {
  for (const PasswordForm& form :
       credential_cache_.GetCredentialStore(URLToOrigin(url))
           .GetUnnotifiedSharedCredentials()) {
    // Make a non-const copy so we can modify it.
    password_manager::PasswordForm updatedForm = form;
    updatedForm.sharing_notification_displayed = true;
    if (updatedForm.IsUsingAccountStore()) {
      GetAccountPasswordStore()->UpdateLogin(std::move(updatedForm));
    } else {
      GetProfilePasswordStore()->UpdateLogin(std::move(updatedForm));
    }
  }
}

#endif  // BUILDFLAG(IS_ANDROID)

version_info::Channel ChromePasswordManagerClient::GetChannel() const {
  return chrome::GetChannel();
}

void ChromePasswordManagerClient::RefreshPasswordManagerSettingsIfNeeded()
    const {
#if BUILDFLAG(IS_ANDROID)
  // TODO (b/334091460): Add
  // password_manager_android_util::AreMinUpmRequirementsMet() check here.
  PasswordManagerSettingsServiceFactory::GetForProfile(profile_)
      ->RequestSettingsFromBackend();
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::OpenPasswordDetailsBubble(
    const PasswordForm& form) {
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  if (manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnOpenPasswordDetailsBubble(form);
  }
}

std::unique_ptr<
    password_manager::PasswordCrossDomainConfirmationPopupController>
ChromePasswordManagerClient::ShowCrossDomainConfirmationPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const GURL& domain,
    const std::u16string& password_origin,
    base::OnceClosure confirmation_callback) {
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  gfx::RectF element_bounds_in_screen_space =
      element_bounds + client_area.OffsetFromOrigin();
  auto controller =
      cross_domain_confirmation_popup_factory_for_testing_
          ? cross_domain_confirmation_popup_factory_for_testing_.Run()
          : std::make_unique<
                PasswordCrossDomainConfirmationPopupControllerImpl>(
                web_contents());

  controller->Show(element_bounds_in_screen_space, text_direction, domain,
                   password_origin, std::move(confirmation_callback));

  return controller;
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ChromePasswordManagerClient::ShowCredentialsInAmbientBubble(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> forms,
    int credential_type_flags,
    CredentialsCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  auto* controller =
      ambient_signin::AmbientSigninController::GetOrCreateForCurrentDocument(
          web_contents()->GetPrimaryMainFrame());
  controller->AddAndShowPasswordMethods(std::move(forms), credential_type_flags,
                                        std::move(callback));
#else
  NOTREACHED_NORETURN();
#endif
}

void ChromePasswordManagerClient::AutomaticGenerationAvailable(
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          rfh, ui_data.form_data.url(),
          BadMessageReason::
              CPMD_BAD_ORIGIN_AUTOMATIC_GENERATION_STATUS_CHANGED)) {
    return;
  }
  if (!password_manager::bad_message::CheckFrameNotPrerendering(rfh)) {
    return;
  }
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  CHECK(driver);
  // This guards against possibility that generation was available on page load
  // but later became unavailable due to inability to save passwords.
  if (!driver->GetPasswordGenerationHelper() ||
      !driver->GetPasswordGenerationHelper()->IsGenerationEnabled(
          /*log_debug_data*/ false)) {
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  if (!ShouldAcceptFocusEvent(web_contents(), driver,
                              FocusedFieldType::kFillablePasswordField)) {
    return;
  }

  PasswordGenerationController* generation_controller =
      PasswordGenerationController::GetOrCreate(web_contents());

  gfx::RectF element_bounds_in_screen_space = TransformToRootCoordinates(
      password_generation_driver_receivers_.GetCurrentTargetFrame(),
      ui_data.bounds);

  auto has_saved_credentials =
      !credential_cache_.GetCredentialStore(rfh->GetLastCommittedOrigin())
           .GetCredentials()
           .empty();
  generation_controller->OnAutomaticGenerationAvailable(
      driver->AsWeakPtrImpl(), ui_data, has_saved_credentials,
      element_bounds_in_screen_space);
  // Trigger password suggestions. This is a fallback case if the field was
  // wrongly classified as new password field.
  driver->GetPasswordAutofillManager()->MaybeShowPasswordSuggestions(
      element_bounds_in_screen_space, ui_data.text_direction);
#else
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
        autofill::mojom::AutofillSuggestionAvailability::kAutofillAvailable);
    return;
  }

  // With `kPasswordGenerationSoftNudge` enabled generated password is previewed
  // when the popup is visible and any character typed by the user is treated as
  // rejection.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordGenerationSoftNudge)) {
    // TODO(crbug.com/366198626): Rewrite the AutomaticGenerationAvailable
    // triggering instead of checking a boolean when the feature is launched.
    if (ui_data.input_field_empty) {
      ShowPasswordGenerationPopup(PasswordGenerationType::kAutomatic, driver,
                                  ui_data);
    } else if (popup_controller_) {
      popup_controller_->GeneratedPasswordRejected();
    }

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
#if BUILDFLAG(IS_ANDROID)
  // The popup obscures part of the page and the bottom sheet already displays
  // the same information before generation.
  return;
#else
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  if (!password_manager::bad_message::CheckFrameNotPrerendering(rfh)) {
    return;
  }
  auto* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  CHECK(driver);

  gfx::RectF element_bounds_in_screen_space =
      GetBoundsInScreenSpace(TransformToRootCoordinates(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          bounds));
  autofill::password_generation::PasswordGenerationUIData ui_data(
      bounds, /*max_length=*/0, /*generation_element=*/std::u16string(),
      field_renderer_id,
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      password_manager::GetFormWithFrameAndFormMetaData(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          form_data),
      /*input_field_empty=*/false);
  popup_controller_ = PasswordGenerationPopupControllerImpl::GetOrCreate(
      popup_controller_, element_bounds_in_screen_space, ui_data,
      driver->AsWeakPtr(), observer_, web_contents(),
      password_generation_driver_receivers_.GetCurrentTargetFrame());
  DCHECK(!password_value.empty());
  popup_controller_->UpdateGeneratedPassword(password_value);
  popup_controller_->Show(
      PasswordGenerationPopupController::kEditGeneratedPassword);
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::PasswordGenerationRejectedByTyping() {
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  if (!password_manager::bad_message::CheckFrameNotPrerendering(rfh)) {
    return;
  }
  if (popup_controller_) {
    popup_controller_->GeneratedPasswordRejected();
  }
}

void ChromePasswordManagerClient::PresaveGeneratedPassword(
    const autofill::FormData& form_data,
    const std::u16string& password_value) {
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  if (!password_manager::bad_message::CheckFrameNotPrerendering(rfh)) {
    return;
  }

  if (popup_controller_) {
    popup_controller_->UpdateGeneratedPassword(password_value);
  }

  PasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  CHECK(driver);
  password_manager_.OnPresaveGeneratedPassword(
      driver,
      password_manager::GetFormWithFrameAndFormMetaData(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          form_data),
      password_value);
}

void ChromePasswordManagerClient::PasswordNoLongerGenerated(
    const autofill::FormData& form_data) {
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  if (!password_manager::bad_message::CheckFrameNotPrerendering(rfh)) {
    return;
  }
  PasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  // This method is called over Mojo via a RenderFrameHostReceiverSet; the
  // current target frame must be live.
  CHECK(driver);
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
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  if (!password_manager::bad_message::CheckFrameNotPrerendering(rfh)) {
    return;
  }
  if (popup_controller_) {
    popup_controller_->FrameWasScrolled();
  }
}

void ChromePasswordManagerClient::GenerationElementLostFocus() {
  content::RenderFrameHost* rfh =
      password_generation_driver_receivers_.GetCurrentTargetFrame();
  if (!password_manager::bad_message::CheckFrameNotPrerendering(rfh)) {
    return;
  }
  // TODO(crbug.com/40629608): Look into removing this since FocusedInputChanged
  // seems to be a good replacement.
  if (popup_controller_) {
    popup_controller_->GenerationElementLostFocus();
  }
}

void ChromePasswordManagerClient::SetTestObserver(
    PasswordGenerationPopupObserver* observer) {
  observer_ = observer;
}

// static
void ChromePasswordManagerClient::BindCredentialManager(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CredentialManager> receiver) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent()) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  // Only valid for the currently committed RenderFrameHost, and not, e.g. old
  // zombie RFH's being swapped out following cross-origin navigations.
  if (web_contents->GetPrimaryMainFrame() != render_frame_host) {
    return;
  }

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
  if (!instance) {
    return;
  }

  // Disable BackForwardCache for this page.
  // This is necessary because ContentCredentialManager::DisconnectBinding()
  // will be called when the page is navigated away from, leaving it
  // in an unusable state if the page is restored from the BackForwardCache.
  //
  // It looks like in order to remove this workaround, we probably just need to
  // make the CredentialManager mojo API rebind on the renderer side when the
  // next call is made, if it has become disconnected.
  // TODO(crbug.com/40653684): Remove this workaround.
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
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>(
        profile_, GetOrCreateKeyboardReplacingSurfaceVisibilityController());
  }
  return touch_to_fill_controller_.get();
}

void ChromePasswordManagerClient::MaybeShowAccountStorageNotice(
    base::OnceClosure callback) {
  // Unretained() is safe because `this` outlives `account_storage_notice_`.
  auto destroy_notice_cb = base::BindOnce(
      [](ChromePasswordManagerClient* client) {
        client->account_storage_notice_.reset();
      },
      base::Unretained(this));
  const bool had_notice = account_storage_notice_.get();
  account_storage_notice_ = AccountStorageNotice::MaybeShow(
      SyncServiceFactory::GetForProfile(profile_), profile_->GetPrefs(),
      web_contents()->GetNativeView()->GetWindowAndroid(),
      std::move(destroy_notice_cb).Then(std::move(callback)));
  // MaybeShow() will return non-null at most once, since this is a one-off
  // notice. So the possible cases are:
  // - `account_storage_notice_` was null and stayed so:  No notice shown, just
  //   invokes `callback`.
  // - `account_storage_notice_` was null and became non-null: Shows the notice.
  // - (Speculative) `account_storage_notice_` was non-null and became null:
  //   Hides the notice and executes `callback`. The alternative would be to
  //   ignore `callback` and wait for `account_storage_notice_` to go away, but
  //   that's dangerous (if there's a bug and `account_storage_notice_` is never
  //   reset, the method would always no-op, breaking the saving/filling
  //   callers).
  CHECK(!had_notice || !account_storage_notice_);
}

password_manager::CredManController*
ChromePasswordManagerClient::GetOrCreateCredManController() {
  if (!cred_man_controller_) {
    cred_man_controller_ =
        std::make_unique<password_manager::CredManController>(
            GetOrCreateKeyboardReplacingSurfaceVisibilityController(), this);
  }
  return cred_man_controller_.get();
}

base::WeakPtr<password_manager::KeyboardReplacingSurfaceVisibilityController>
ChromePasswordManagerClient::
    GetOrCreateKeyboardReplacingSurfaceVisibilityController() {
  if (!keyboard_replacing_surface_visibility_controller_) {
    keyboard_replacing_surface_visibility_controller_ = std::make_unique<
        password_manager::KeyboardReplacingSurfaceVisibilityControllerImpl>();
  }
  return keyboard_replacing_surface_visibility_controller_->AsWeakPtr();
}
#endif  // BUILDFLAG(IS_ANDROID)

ChromePasswordManagerClient::ChromePasswordManagerClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromePasswordManagerClient>(*web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      password_manager_(this),
      password_feature_manager_(profile_->GetPrefs(),
                                g_browser_process->local_state(),
                                SyncServiceFactory::GetForProfile(profile_)),
      httpauth_manager_(this),
      content_credential_manager_(this),
      password_generation_driver_receivers_(web_contents, this),
      observer_(nullptr),
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      credentials_filter_(
          this,
          DiceWebSigninInterceptorFactory::GetForProfile(profile_)),
#else
      credentials_filter_(this),
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
      account_storage_auth_helper_(
          profile_,
          IdentityManagerFactory::GetForProfile(profile_),
          &password_feature_manager_,
          base::BindRepeating(
              [](content::WebContents* web_contents) {
                Browser* browser = chrome::FindBrowserWithTab(web_contents);
                return browser ? browser->signin_view_controller() : nullptr;
              },
              web_contents)),
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
      helper_(this) {
  ContentPasswordManagerDriverFactory::CreateForWebContents(web_contents, this);
  ContentPasswordManagerDriverFactory* driver_factory = GetDriverFactory();
  log_manager_ = autofill::LogManager::Create(
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          profile_),
      base::BindRepeating(
          &ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability,
          base::Unretained(driver_factory)));

  driver_factory->RequestSendLoggingAvailability();

  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);

#if BUILDFLAG(IS_ANDROID)
  // `this` is tab-scoped, however the local passwords migration warning
  // should only be launched on startup.
  static bool tried_launching_warning_on_startup = false;
  if (!tried_launching_warning_on_startup) {
    tried_launching_warning_on_startup = true;
    TryToShowLocalPasswordMigrationWarning();
  }
  // This prevents the post migration sheet from trying to show on opening new
  // tabs after the initial attempt to show the sheet on startup.
  static bool tried_launching_post_migration_sheet_on_startup = false;
  if (!tried_launching_post_migration_sheet_on_startup) {
    tried_launching_post_migration_sheet_on_startup = true;
    TryToShowPostPasswordMigrationSheet();
  }
  // This prevents the access loss warning from trying to show on opening new
  // tabs after the initial attempt to show the sheet on startup.
  static bool tried_launching_access_loss_warning_on_startup = false;
  if (!tried_launching_access_loss_warning_on_startup) {
    tried_launching_access_loss_warning_on_startup = true;
    TryToShowAccessLossWarningSheet();
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePasswordManagerClient::PrimaryPageChanged(content::Page& page) {
#if BUILDFLAG(IS_ANDROID)
  if (first_cct_page_load_metrics_recorder_) {
    first_cct_page_load_metrics_recorder_.reset();
  } else {
    bool first_cct_page_load =
        FirstCctPageLoadMarker::ConsumeMarker(web_contents());
    TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
    if (tab_android && tab_android->IsCustomTab() && first_cct_page_load) {
      first_cct_page_load_metrics_recorder_ = std::make_unique<
          password_manager::FirstCctPageLoadPasswordsUkmRecorder>(
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
  // Logging has no sense on WebUI sites.
  log_manager_->SetSuspended(web_contents()->GetWebUI() != nullptr);

  // Send any collected metrics by destroying the metrics recorder.
  metrics_recorder_.reset();

  httpauth_manager_.OnDidFinishMainFrameNavigation();

  // From this point on, the ContentCredentialManager will service API calls
  // in the context of the new WebContents::GetLastCommittedURL, which may
  // very well be cross-origin. Disconnect existing client, and drop pending
  // requests.
  content_credential_manager_.DisconnectBinding();

#if BUILDFLAG(IS_ANDROID)
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

void ChromePasswordManagerClient::OnFieldTypesDetermined(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    FieldTypeSource source) {
  if (source == FieldTypeSource::kHeuristicsOrAutocomplete) {
    return;
  }

  std::optional<autofill::RendererFormsWithServerPredictions>
      forms_and_predictions =
          autofill::RendererFormsWithServerPredictions::FromBrowserForm(
              manager, form_id);
  if (!forms_and_predictions) {
    return;
  }

  for (const auto& [form, rfh_id] : forms_and_predictions->renderer_forms) {
    auto* rfh = content::RenderFrameHost::FromID(rfh_id);
    if (!rfh) {
      continue;
    }
    auto* driver =
        password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
            rfh);
    if (!driver) {
      continue;
    }
    password_manager_.ProcessAutofillPredictions(
        driver, form, forms_and_predictions->predictions);
  }
}

password_manager::ContentPasswordManagerDriverFactory*
ChromePasswordManagerClient::GetDriverFactory() const {
  return password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
      web_contents());
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
  if (mf_controller) {
    mf_controller->Hide();
  }

  PasswordGenerationController* generation_controller =
      PasswordGenerationController::GetIfExisting(web_contents());
  if (generation_controller) {
    generation_controller->HideBottomSheetIfNeeded();
  }
  if (touch_to_fill_controller_) {
    touch_to_fill_controller_->Reset();
  }

  if (cred_man_controller_) {
    cred_man_controller_.reset();
  }

  if (keyboard_replacing_surface_visibility_controller_) {
    keyboard_replacing_surface_visibility_controller_->Reset();
  }
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

void ChromePasswordManagerClient::GenerationResultAvailable(
    PasswordGenerationType type,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
    const std::optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data) {
  if (!ui_data || !driver) {
    return;
  }
  // Check the data because it's a Mojo callback and the input isn't trusted.
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          driver->render_frame_host(), ui_data->form_data.url(),
          BadMessageReason::
              CPMD_BAD_ORIGIN_SHOW_MANUAL_PASSWORD_GENERATION_POPUP)) {
    return;
  }
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
      driver, ui_data.form_data.renderer_id(), ui_data.generation_element_id,
      type);

  popup_controller_ = PasswordGenerationPopupControllerImpl::GetOrCreate(
      popup_controller_, element_bounds_in_screen_space, ui_data,
      driver->AsWeakPtr(), observer_, web_contents(),
      driver->render_frame_host());

  popup_controller_->GeneratePasswordValue(type);
  popup_controller_->Show(PasswordGenerationPopupController::kOfferGeneration);

  driver->SetSuggestionAvailability(
      ui_data.generation_element_id,
      popup_controller_ && popup_controller_->IsVisible()
          ? autofill::mojom::AutofillSuggestionAvailability::kAutofillAvailable
          : autofill::mojom::AutofillSuggestionAvailability::kNoSuggestions);
}

gfx::RectF ChromePasswordManagerClient::TransformToRootCoordinates(
    content::RenderFrameHost* frame_host,
    const gfx::RectF& bounds_in_frame_coordinates) {
  content::RenderWidgetHostView* rwhv = frame_host->GetView();
  if (!rwhv) {
    return bounds_in_frame_coordinates;
  }
  return gfx::RectF(rwhv->TransformPointToRootCoordSpaceF(
                        bounds_in_frame_coordinates.origin()),
                    bounds_in_frame_coordinates.size());
}

#if BUILDFLAG(IS_ANDROID)
void ChromePasswordManagerClient::ResetErrorMessageDelegate() {
  password_manager_error_message_delegate_.reset();
}

void ChromePasswordManagerClient::TryToShowLocalPasswordMigrationWarning() {
  password_manager::PasswordStoreInterface* profile_password_store =
      GetProfilePasswordStore();
  if (profile_password_store == nullptr) {
    return;
  }
  password_migration_warning_startup_launcher_ =
      std::make_unique<PasswordMigrationWarningStartupLauncher>(
          web_contents(), profile_,
          base::BindOnce(&local_password_migration::ShowWarning));
  password_migration_warning_startup_launcher_
      ->MaybeFetchPasswordsAndShowWarning(profile_password_store);
}

void ChromePasswordManagerClient::TryToShowPostPasswordMigrationSheet() {
  // This is to let the method run after all the initialization tasks have been
  // completed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MaybeShowPostMigrationSheetWrapper,
                                web_contents()->GetWeakPtr(), profile_));
}

void ChromePasswordManagerClient::TryToShowAccessLossWarningSheet() {
  // This is to let the method run after all the initialization tasks have been
  // completed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MaybeShowAccessLossWarning, GetPrefs(),
                                web_contents()->GetWeakPtr(), profile_));
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromePasswordManagerClient);
