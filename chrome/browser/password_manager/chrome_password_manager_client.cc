// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <memory>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_biometric_authenticator.h"
#include "chrome/browser/password_manager/field_info_manager_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/content/browser/password_requirements_service_factory.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "components/password_manager/core/browser/store_metrics_reporter.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
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
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/url_constants.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/events/keycodes/keyboard_codes.h"
#endif

#if defined(OS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/android/account_chooser_dialog_android.h"
#include "chrome/browser/password_manager/android/auto_signin_first_run_dialog_android.h"
#include "chrome/browser/password_manager/android/auto_signin_prompt_controller.h"
#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"
#include "chrome/browser/password_manager/android/generated_password_saved_infobar_delegate_android.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_controller_impl.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/password_manager/android/save_password_infobar_delegate_android.h"
#include "chrome/browser/password_manager/android/update_password_infobar_delegate_android.h"
#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "components/infobars/core/infobar.h"
#include "components/messages/android/messages_feature.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "ui/base/ui_base_features.h"
#else
#include "chrome/browser/ui/browser_finder.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/scoped_allow_sync_call.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#endif

#if defined(OS_ANDROID)
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
using password_manager::metrics_util::PasswordType;
using sessions::SerializedNavigationEntry;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace {

const syncer::SyncService* GetSyncService(Profile* profile) {
  if (ProfileSyncServiceFactory::HasSyncService(profile))
    return ProfileSyncServiceFactory::GetForProfile(profile);
  return nullptr;
}

// Adds |observer| to the input observers of |widget_host|.
void AddToWidgetInputEventObservers(
    content::RenderWidgetHost* widget_host,
    content::RenderWidgetHost::InputEventObserver* observer) {
  // Since Widget API doesn't allow to check whether the observer is already
  // added, the observer is removed and added again, to ensure that it is added
  // only once.
#if defined(OS_ANDROID)
  widget_host->RemoveImeInputEventObserver(observer);
  widget_host->AddImeInputEventObserver(observer);
#endif
  widget_host->RemoveInputEventObserver(observer);
  widget_host->AddInputEventObserver(observer);
}

#if defined(OS_ANDROID)
void HideSavePasswordInfobar(content::WebContents* web_contents) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        SavePasswordInfoBarDelegate::SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE) {
      infobar_service->RemoveInfoBar(infobar);
      break;
    }
  }
}
#endif  // defined(OS_ANDROID)

class NavigationPasswordMetricsRecorder
    : public PasswordManagerMetricsRecorder::NavigationMetricRecorderDelegate {
 public:
  explicit NavigationPasswordMetricsRecorder(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  void OnUserFocusedPasswordFieldFirstTime() override {
    RecordEngagementLevel("Security.PasswordFocus.SiteEngagementLevel");
  }

  void OnUserModifiedPasswordFieldFirstTime() override {
    RecordEngagementLevel("Security.PasswordEntry.SiteEngagementLevel");
  }

 private:
  void RecordEngagementLevel(const char* histogram_name) {
    const GURL& main_frame_url = web_contents_->GetLastCommittedURL();
    if (main_frame_url.SchemeIsHTTPOrHTTPS()) {
      site_engagement::SiteEngagementService* site_engagement_service =
          site_engagement::SiteEngagementService::Get(
              Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
      blink::mojom::EngagementLevel engagement_level =
          site_engagement_service->GetEngagementLevel(main_frame_url);
      base::UmaHistogramEnumeration(histogram_name, engagement_level);
    }
  }

  content::WebContents* web_contents_;
};

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

ChromePasswordManagerClient::~ChromePasswordManagerClient() = default;

bool ChromePasswordManagerClient::IsSavingAndFillingEnabled(
    const GURL& url) const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    // Disable the password saving UI for automated tests. It obscures the
    // page, and there is no API to access (or dismiss) UI bubbles/infobars.
    return false;
  }
  return *saving_passwords_enabled_ && !IsIncognito() && IsFillingEnabled(url);
}

bool ChromePasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  const bool ssl_errors = net::IsCertStatusError(GetMainFrameCertStatus());

  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogBoolean(Logger::STRING_SSL_ERRORS_PRESENT, ssl_errors);
  }

  return !ssl_errors && IsPasswordManagementEnabledForCurrentPage(url);
}

bool ChromePasswordManagerClient::IsFillingFallbackEnabled(
    const GURL& url) const {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return IsFillingEnabled(url) && !profile->IsGuestSession() &&
         !profile->IsEphemeralGuestProfile();
}

bool ChromePasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  // The save password infobar and the password bubble prompt in case of
  // "webby" URLs and do not prompt in case of "non-webby" URLS (e.g. file://).
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return false;

#if defined(OS_ANDROID)
  if (form_to_save->IsBlocklisted())
    return false;

  if (update_password) {
    UpdatePasswordInfoBarDelegate::Create(web_contents(),
                                          std::move(form_to_save));
  } else {
    if (messages::IsPasswordMessagesUiEnabled()) {
      save_password_message_delegate_.DisplaySavePasswordPrompt(
          web_contents(), std::move(form_to_save));
    } else {
      SavePasswordInfoBarDelegate::Create(web_contents(),
                                          std::move(form_to_save));
    }
  }
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
#if !defined(OS_ANDROID)
  PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnShowMoveToAccountBubble(std::move(form_to_move));
#endif  // !defined(OS_ANDROID)
}

void ChromePasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
#if !defined(OS_ANDROID)
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
#endif  // !defined(OS_ANDROID)
}

void ChromePasswordManagerClient::HideManualFallbackForSaving() {
#if !defined(OS_ANDROID)
  if (!CanShowBubbleOnURL(web_contents()->GetLastCommittedURL()))
    return;

  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  // There may be no UI controller for ChromeOS login page
  // (see crbug.com/774676).
  if (manage_passwords_ui_controller)
    manage_passwords_ui_controller->OnHideManualFallbackForSaving();
#endif  // !defined(OS_ANDROID)
}

void ChromePasswordManagerClient::FocusedInputChanged(
    PasswordManagerDriver* driver,
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {
#if defined(OS_ANDROID)
  ManualFillingController::GetOrCreate(web_contents())
      ->NotifyFocusedInputChanged(focused_field_id, focused_field_type);
  password_manager::ContentPasswordManagerDriver* content_driver =
      static_cast<password_manager::ContentPasswordManagerDriver*>(driver);
  if (!PasswordAccessoryControllerImpl::ShouldAcceptFocusEvent(
          web_contents(), content_driver, focused_field_type))
    return;

  if (!content_driver->CanShowAutofillUi())
    return;

  if (!PasswordAccessoryController::AllowedForWebContents(web_contents()))
    return;

  if (web_contents()->GetFocusedFrame()) {
    GetOrCreatePasswordAccessory()->RefreshSuggestionsForField(
        focused_field_type,
        password_manager_util::ManualPasswordGenerationEnabled(driver));
  }

  PasswordGenerationController::GetOrCreate(web_contents())
      ->FocusedInputChanged(focused_field_type,
                            base::AsWeakPtr(content_driver));
#endif  // defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
  // Deletes itself on the event from Java counterpart, when user interacts with
  // dialog.
  AccountChooserDialogAndroid* acccount_chooser_dialog =
      new AccountChooserDialogAndroid(web_contents(), std::move(local_forms),
                                      origin, std::move(intercept));
  return acccount_chooser_dialog->ShowDialog();
#else
  return PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnChooseCredentials(std::move(local_forms), origin,
                            std::move(intercept));
#endif
}

void ChromePasswordManagerClient::ShowTouchToFill(
    PasswordManagerDriver* driver) {
#if defined(OS_ANDROID)
  GetOrCreateTouchToFillController()->Show(
      credential_cache_
          .GetCredentialStore(
              url::Origin::Create(driver->GetLastCommittedURL().GetOrigin()))
          .GetCredentials(),
      driver->AsWeakPtr());
#endif
}

#if defined(OS_ANDROID)
void ChromePasswordManagerClient::OnPasswordSelected(
    const std::u16string& text) {
  password_reuse_detection_manager_.OnPaste(text);
}
#endif

bool ChromePasswordManagerClient::IsAutofillAssistantUIVisible() const {
  auto* autofill_assistant_manager =
      autofill_assistant::RuntimeManager::GetForWebContents(web_contents());
  return autofill_assistant_manager && autofill_assistant_manager->GetState() ==
                                           autofill_assistant::UIState::kShown;
}

password_manager::BiometricAuthenticator*
ChromePasswordManagerClient::GetBiometricAuthenticator() {
#if defined(OS_ANDROID)
  if (!biometric_authenticator_) {
    biometric_authenticator_ =
        ChromeBiometricAuthenticator::Create(web_contents());
  }
#endif
  return biometric_authenticator_.get();
}

void ChromePasswordManagerClient::GeneratePassword(
    PasswordGenerationType type) {
#if defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
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

void ChromePasswordManagerClient::UpdateCredentialCache(
    const url::Origin& origin,
    const std::vector<const PasswordForm*>& best_matches,
    bool is_blocklisted) {
#if defined(OS_ANDROID)
  credential_cache_.SaveCredentialsAndBlocklistedForOrigin(
      best_matches, CredentialCache::IsOriginBlocklisted(is_blocklisted),
      origin);

#endif
}

void ChromePasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form) {
#if defined(OS_ANDROID)
  if (messages::IsPasswordMessagesUiEnabled()) {
    generated_password_saved_message_delegate_.ShowPrompt(
        web_contents(), std::move(saved_form));
  } else {
    GeneratedPasswordSavedInfoBarDelegateAndroid::Create(web_contents());
  }
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
    const std::vector<const PasswordForm*>* federated_matches) {
#if !defined(OS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnPasswordAutofilled(best_matches, origin,
                                                       federated_matches);
#endif
}

void ChromePasswordManagerClient::AutofillHttpAuth(
    const PasswordForm& preferred_match,
    const password_manager::PasswordFormManagerForUI* form_manager) {
  httpauth_manager_.Autofill(preferred_match, form_manager);
  DCHECK(!form_manager->GetBestMatches().empty());
  PasswordWasAutofilled(form_manager->GetBestMatches(),
                        url::Origin::Create(form_manager->GetURL()), nullptr);
}

void ChromePasswordManagerClient::NotifyUserCredentialsWereLeaked(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin,
    const std::u16string& username) {
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordScriptsFetching) &&
      GetPasswordFeatureManager()->IsGenerationEnabled()) {
    PasswordScriptsFetcherFactory::GetInstance()
        ->GetForBrowserContext(web_contents()->GetBrowserContext())
        ->PrewarmCache();
  }
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordChange)) {
    was_leak_dialog_shown_ = true;
  }

  if (messages::IsPasswordMessagesUiEnabled()) {
    save_password_message_delegate_.DismissSavePasswordPrompt();
  } else {
    HideSavePasswordInfobar(web_contents());
  }

  (new CredentialLeakControllerAndroid(
       leak_type, origin, username, web_contents()->GetTopLevelNativeWindow()))
      ->ShowDialog();
#else   // !defined(OS_ANDROID)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  manage_passwords_ui_controller->OnCredentialLeak(leak_type, origin);
#endif  // defined(OS_ANDROID)
}

void ChromePasswordManagerClient::TriggerReauthForPrimaryAccount(
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback) {
#if defined(OS_ANDROID)
  std::move(reauth_callback).Run(ReauthSucceeded(false));
#else   // !defined(OS_ANDROID)
  account_storage_auth_helper_.TriggerOptInReauth(access_point,
                                                  std::move(reauth_callback));
#endif  // defined(OS_ANDROID)
}

void ChromePasswordManagerClient::TriggerSignIn(
    signin_metrics::AccessPoint access_point) {
#if !defined(OS_ANDROID)
  account_storage_auth_helper_.TriggerSignIn(access_point);
#endif
}

PrefService* ChromePasswordManagerClient::GetPrefs() const {
  return profile_->GetPrefs();
}

password_manager::PasswordStore*
ChromePasswordManagerClient::GetProfilePasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  return PasswordStoreFactory::GetForProfile(profile_,
                                             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStore*
ChromePasswordManagerClient::GetAccountPasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  return AccountPasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::SyncState ChromePasswordManagerClient::GetPasswordSyncState()
    const {
  const syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
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

#if defined(OS_ANDROID)
bool ChromePasswordManagerClient::WasCredentialLeakDialogShown() const {
  return was_leak_dialog_shown_;
}
#endif  // defined(OS_ANDROID)

net::CertStatus ChromePasswordManagerClient::GetMainFrameCertStatus() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return 0;
  return entry->GetSSL().cert_status;
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

bool ChromePasswordManagerClient::IsIncognito() const {
  // TODO(https://crbug.com/1125474): After deprecating off-the-record Guest
  // profile, update this function for better readability.
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  return browser_context->IsOffTheRecord() ||
         profile->IsEphemeralGuestProfile();
}

profile_metrics::BrowserProfileType
ChromePasswordManagerClient::GetProfileType() const {
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  return ProfileMetrics::GetBrowserProfileType(
      Profile::FromBrowserContext(browser_context));
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
        factory->DriverForFrame(web_contents()->GetMainFrame());
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
      web_contents()->GetMainFrame()->GetLastCommittedOrigin());
}

const GURL& ChromePasswordManagerClient::GetLastCommittedURL() const {
  return web_contents()->GetLastCommittedURL();
}

url::Origin ChromePasswordManagerClient::GetLastCommittedOrigin() const {
  DCHECK(web_contents());
  return web_contents()->GetMainFrame()->GetLastCommittedOrigin();
}
const password_manager::CredentialsFilter*
ChromePasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

const autofill::LogManager* ChromePasswordManagerClient::GetLogManager() const {
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
  }
}

autofill::LanguageCode ChromePasswordManagerClient::GetPageLanguage() const {
  // TODO(crbug.com/912597): iOS vs other platforms extracts language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents());
  if (translate_manager)
    return autofill::LanguageCode(
        translate_manager->GetLanguageState()->original_language());
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
    bool password_field_exists) {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (!pps)
    return;

  pps->MaybeStartProtectedPasswordEntryRequest(
      web_contents(), web_contents()->GetLastCommittedURL(), username,
      password_type, matching_reused_credentials, password_field_exists);
}

void ChromePasswordManagerClient::LogPasswordReuseDetectedEvent() {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (pps) {
    pps->MaybeLogPasswordReuseDetectedEvent(web_contents());
  }
}

ukm::SourceId ChromePasswordManagerClient::GetUkmSourceId() {
  return ukm::GetSourceIdForWebContentsDocument(web_contents());
}

PasswordManagerMetricsRecorder*
ChromePasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(
        GetUkmSourceId(),
        std::make_unique<NavigationPasswordMetricsRecorder>(web_contents()));
  }
  return base::OptionalOrNullptr(metrics_recorder_);
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
  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetURLLoaderFactoryForBrowserProcess();
}

network::mojom::NetworkContext* ChromePasswordManagerClient::GetNetworkContext()
    const {
  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetNetworkContext();
}

bool ChromePasswordManagerClient::IsUnderAdvancedProtection() const {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
             profile_)
      ->IsUnderAdvancedProtection();
#else
  return false;
#endif
}

void ChromePasswordManagerClient::UpdateFormManagers() {
  password_manager_.UpdateFormManagers();
}

void ChromePasswordManagerClient::NavigateToManagePasswordsPage(
    password_manager::ManagePasswordsReferrer referrer) {
#if defined(OS_ANDROID)
  password_manager_launcher::ShowPasswordSettings(web_contents(), referrer);
#else
  ::NavigateToManagePasswordsPage(
      chrome::FindBrowserWithWebContents(web_contents()), referrer);
#endif
}

bool ChromePasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  // TODO(crbug.com/862989): Move the following function (and the feature) to
  // the password component. Then remove IsIsolationForPasswordsSitesEnabled()
  // from the PasswordManagerClient interface.
  return site_isolation::SiteIsolationPolicy::
      IsIsolationForPasswordSitesEnabled();
}

bool ChromePasswordManagerClient::IsNewTabPage() const {
  auto origin = GetLastCommittedURL().GetOrigin();
  return origin == GURL(chrome::kChromeUINewTabPageURL).GetOrigin() ||
         origin == GURL(chrome::kChromeUINewTabURL).GetOrigin();
}

FieldInfoManager* ChromePasswordManagerClient::GetFieldInfoManager() const {
  return FieldInfoManagerFactory::GetForBrowserContext(profile_);
}

void ChromePasswordManagerClient::AutomaticGenerationAvailable(
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          ui_data.form_data.url,
          BadMessageReason::
              CPMD_BAD_ORIGIN_AUTOMATIC_GENERATION_STATUS_CHANGED))
    return;
#if defined(OS_ANDROID)
  if (PasswordGenerationController::AllowedForWebContents(web_contents())) {
    PasswordManagerDriver* driver = driver_factory_->GetDriverForFrame(
        password_generation_driver_receivers_.GetCurrentTargetFrame());

    PasswordGenerationController* generation_controller =
        PasswordGenerationController::GetIfExisting(web_contents());
    DCHECK(generation_controller);

    gfx::RectF element_bounds_in_screen_space = TransformToRootCoordinates(
        password_generation_driver_receivers_.GetCurrentTargetFrame(),
        ui_data.bounds);

    generation_controller->OnAutomaticGenerationAvailable(
        driver, ui_data, element_bounds_in_screen_space);
  }
#else
  password_manager::ContentPasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(
          password_generation_driver_receivers_.GetCurrentTargetFrame());

  // Attempt to show the autofill dropdown UI first.
  gfx::RectF element_bounds_in_top_frame_space =
      TransformToRootCoordinates(driver->render_frame_host(), ui_data.bounds);
  if (driver->GetPasswordAutofillManager()
          ->MaybeShowPasswordSuggestionsWithGeneration(
              element_bounds_in_top_frame_space, ui_data.text_direction,
              /*show_password_suggestions=*/
              ui_data.is_generation_element_password_type)) {
    return;
  }

  ShowPasswordGenerationPopup(PasswordGenerationType::kAutomatic, driver,
                              ui_data);
#endif  // defined(OS_ANDROID)
}

void ChromePasswordManagerClient::ShowPasswordEditingPopup(
    const gfx::RectF& bounds,
    const autofill::FormData& form_data,
    autofill::FieldRendererId field_renderer_id,
    const std::u16string& password_value) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          form_data.url,
          BadMessageReason::CPMD_BAD_ORIGIN_SHOW_PASSWORD_EDITING_POPUP))
    return;
  auto* driver = driver_factory_->GetDriverForFrame(
      password_generation_driver_receivers_.GetCurrentTargetFrame());
  gfx::RectF element_bounds_in_screen_space =
      GetBoundsInScreenSpace(TransformToRootCoordinates(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          bounds));
  autofill::password_generation::PasswordGenerationUIData ui_data(
      bounds, /*max_length=*/0, /*generation_element=*/std::u16string(),
      field_renderer_id, /*is_generation_element_password_type=*/true,
      base::i18n::TextDirection(), form_data);
  popup_controller_ = PasswordGenerationPopupControllerImpl::GetOrCreate(
      popup_controller_, element_bounds_in_screen_space, ui_data,
      driver->AsWeakPtr(), observer_, web_contents(),
      password_generation_driver_receivers_.GetCurrentTargetFrame());
  DCHECK(!password_value.empty());
  popup_controller_->UpdatePassword(password_value);
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
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          form_data.url,
          BadMessageReason::CPMD_BAD_ORIGIN_PRESAVE_GENERATED_PASSWORD)) {
    return;
  }
  if (popup_controller_)
    popup_controller_->UpdatePassword(password_value);

  PasswordManagerDriver* driver = driver_factory_->GetDriverForFrame(
      password_generation_driver_receivers_.GetCurrentTargetFrame());
  password_manager_.OnPresaveGeneratedPassword(driver, form_data,
                                               password_value);
}

void ChromePasswordManagerClient::PasswordNoLongerGenerated(
    const autofill::FormData& form_data) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          password_generation_driver_receivers_.GetCurrentTargetFrame(),
          form_data.url,
          BadMessageReason::CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED)) {
    return;
  }
  PasswordManagerDriver* driver = driver_factory_->GetDriverForFrame(
      password_generation_driver_receivers_.GetCurrentTargetFrame());
  password_manager_.OnPasswordNoLongerGenerated(driver, form_data);

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

#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)

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
  if (web_contents->GetMainFrame() != render_frame_host)
    return;

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

#if defined(OS_ANDROID)
PasswordAccessoryController*
ChromePasswordManagerClient::GetOrCreatePasswordAccessory() {
  return PasswordAccessoryController::GetOrCreate(web_contents(),
                                                  &credential_cache_);
}

TouchToFillController*
ChromePasswordManagerClient::GetOrCreateTouchToFillController() {
  if (!touch_to_fill_controller_)
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>(this);

  return touch_to_fill_controller_.get();
}
#endif  // defined(OS_ANDROID)

ChromePasswordManagerClient::ChromePasswordManagerClient(
    content::WebContents* web_contents,
    autofill::AutofillClient* autofill_client)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      password_manager_(this),
      password_feature_manager_(
          profile_->GetPrefs(),
          ProfileSyncServiceFactory::GetForProfile(profile_)),
      httpauth_manager_(this),
      password_reuse_detection_manager_(this),
      driver_factory_(nullptr),
      content_credential_manager_(this),
      password_generation_driver_receivers_(web_contents, this),
      observer_(nullptr),
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_ANDROID)
      credentials_filter_(this, base::BindRepeating(&GetSyncService, profile_)),
#else
      credentials_filter_(
          this,
          base::BindRepeating(&GetSyncService, profile_),
          DiceWebSigninInterceptorFactory::GetForProfile(profile_)),
#endif
#if !defined(OS_ANDROID)
      account_storage_auth_helper_(profile_, &password_feature_manager_),
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

  saving_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, GetPrefs());
  static base::NoDestructor<password_manager::StoreMetricsReporter> reporter(
      this, GetSyncService(profile_), GetIdentityManager(), GetPrefs());
  driver_factory_->RequestSendLoggingAvailability();

  auto* autofill_assistant_manager =
      autofill_assistant::RuntimeManager::GetOrCreateForWebContents(
          web_contents);
  autofill_assistant_manager->AddObserver(this);
}

void ChromePasswordManagerClient::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Logging has no sense on WebUI sites.
  log_manager_->SetSuspended(web_contents()->GetWebUI() != nullptr);
}

void ChromePasswordManagerClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Send any collected metrics by destroying the metrics recorder.
  metrics_recorder_.reset();

  httpauth_manager_.OnDidFinishMainFrameNavigation();

  // From this point on, the ContentCredentialManager will service API calls in
  // the context of the new WebContents::GetLastCommittedURL, which may very
  // well be cross-origin. Disconnect existing client, and drop pending
  // requests.
  content_credential_manager_.DisconnectBinding();

  password_reuse_detection_manager_.DidNavigateMainFrame(GetLastCommittedURL());

  AddToWidgetInputEventObservers(
      web_contents()->GetMainFrame()->GetRenderViewHost()->GetWidget(), this);
#if defined(OS_ANDROID)
  // This unblocklisted info is only used after form submission to determine
  // whether to record PasswordManager.SaveUIDismissalReasonAfterUnblacklisting.
  // Therefore it is sufficient to save it only once on navigation and not
  // every time the user changes the UI toggle.
  password_manager_.MarkWasUnblocklistedInFormManagers(&credential_cache_);
  credential_cache_.ClearCredentials();
#endif  // defined(OS_ANDROID)

  // Hide form filling UI on navigating away.
  HideFillingUI();
}

void ChromePasswordManagerClient::WebContentsDestroyed() {
  // crbug/1090011
  // Drop the connection before the WebContentsObserver destructors are invoked.
  // Other classes may contain callbacks to the Mojo methods. Those callbacks
  // don't like to be destroyed earlier than the pipe itself.
  content_credential_manager_.DisconnectBinding();

  auto* autofill_assistant_manager =
      autofill_assistant::RuntimeManager::GetForWebContents(web_contents());
  if (autofill_assistant_manager) {
    autofill_assistant_manager->RemoveObserver(this);
  }
}

#if !defined(OS_ANDROID)
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
  auto* service = chromeos::LacrosChromeServiceImpl::Get();
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
#endif

void ChromePasswordManagerClient::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // TODO(drubery): We should handle input events on subframes separately, so
  // that we can accurately report that the password was reused on a subframe.
  // Currently any password reuse for this WebContents will report password
  // reuse on the main frame URL.
  AddToWidgetInputEventObservers(
      render_frame_host->GetView()->GetRenderWidgetHost(), this);
}

void ChromePasswordManagerClient::OnInputEvent(
    const blink::WebInputEvent& event) {
#if defined(OS_ANDROID)

  // On Android, key down events are triggered if a user types in through a
  // number bar on Android keyboard. If text is typed in through other parts of
  // Android keyboard, ImeTextCommittedEvent is triggered instead.
  if (event.GetType() != blink::WebInputEvent::Type::kKeyDown)
    return;
  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);
  password_reuse_detection_manager_.OnKeyPressedCommitted(key_event.text);

#else   // !defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)
}

gfx::RectF ChromePasswordManagerClient::GetBoundsInScreenSpace(
    const gfx::RectF& bounds) {
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  return bounds + client_area.OffsetFromOrigin();
}

void ChromePasswordManagerClient::HideFillingUI() {
#if defined(OS_ANDROID)
  base::WeakPtr<ManualFillingController> mf_controller =
      ManualFillingController::Get(web_contents());
  // Hides all the manual filling UI if the controller already exists.
  if (mf_controller)
    mf_controller->Hide();
#endif  // defined(OS_ANDROID)
}

bool ChromePasswordManagerClient::IsPasswordManagementEnabledForCurrentPage(
    const GURL& url) const {
  bool is_enabled = CanShowBubbleOnURL(url);

  // The password manager is disabled while VR (virtual reality) is being used,
  // as the use of conventional UI elements might harm the user experience in
  // VR.
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          web_contents(), vr::UiSuppressedElement::kPasswordManager)) {
    is_enabled = false;
  }

  // The password manager is disabled on Google Password Manager page.
  if (url.GetOrigin() ==
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
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!sync_service || !sync_service->IsSyncFeatureActive() ||
      sync_service->GetUserSettings()->IsUsingSecondaryPassphrase()) {
    return false;
  }

  return true;
}

void ChromePasswordManagerClient::GenerationResultAvailable(
    PasswordGenerationType type,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
    const base::Optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data) {
  if (!ui_data || !driver)
    return;
  // Check the data because it's a Mojo callback and the input isn't trusted.
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          driver->render_frame_host(), ui_data->form_data.url,
          BadMessageReason::
              CPMD_BAD_ORIGIN_SHOW_MANUAL_PASSWORD_GENERATION_POPUP))
    return;
#if defined(OS_ANDROID)
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
  popup_controller_->Show(PasswordGenerationPopupController::kOfferGeneration);
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

void ChromePasswordManagerClient::OnStateChanged(
    autofill_assistant::UIState state) {
  if (state == autofill_assistant::UIState::kNotShown)
    password_manager_.ResetPendingCredentials();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromePasswordManagerClient)
