// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_sender_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/webauthn/change_pin_controller.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/sharing/recipients_fetcher_impl.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils_chromeos.h"
#endif

namespace {

using password_manager::CredentialFacet;
using password_manager::CredentialUIEntry;
using password_manager::FetchFamilyMembersRequestStatus;
using password_manager::constants::kPasswordManagerAuthValidity;

// The error message returned to the UI when Chrome refuses to start multiple
// exports.
const char kExportInProgress[] = "in-progress";
// The error message returned to the UI when the user fails to reauthenticate.
const char kReauthenticationFailed[] = "reauth-failed";

// Map password_manager::ExportProgressStatus to
// extensions::api::passwords_private::ExportProgressStatus.
extensions::api::passwords_private::ExportProgressStatus ConvertStatus(
    password_manager::ExportProgressStatus status) {
  switch (status) {
    case password_manager::ExportProgressStatus::kNotStarted:
      return extensions::api::passwords_private::ExportProgressStatus::
          kNotStarted;
    case password_manager::ExportProgressStatus::kInProgress:
      return extensions::api::passwords_private::ExportProgressStatus::
          kInProgress;
    case password_manager::ExportProgressStatus::kSucceeded:
      return extensions::api::passwords_private::ExportProgressStatus::
          kSucceeded;
    case password_manager::ExportProgressStatus::kFailedCancelled:
      return extensions::api::passwords_private::ExportProgressStatus::
          kFailedCancelled;
    case password_manager::ExportProgressStatus::kFailedWrite:
      return extensions::api::passwords_private::ExportProgressStatus::
          kFailedWriteFailed;
  }

  NOTREACHED_IN_MIGRATION();
  return extensions::api::passwords_private::ExportProgressStatus::kNone;
}

std::u16string GetReauthPurpose(
    extensions::api::passwords_private::PlaintextReason reason) {
#if BUILDFLAG(IS_MAC)

  switch (reason) {
    case extensions::api::passwords_private::PlaintextReason::kView:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case extensions::api::passwords_private::PlaintextReason::kCopy:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_COPY_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case extensions::api::passwords_private::PlaintextReason::kEdit:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EDIT_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case extensions::api::passwords_private::PlaintextReason::kNone:
      NOTREACHED();
  }
#elif BUILDFLAG(IS_WIN)
  switch (reason) {
    case extensions::api::passwords_private::PlaintextReason::kView:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
    case extensions::api::passwords_private::PlaintextReason::kCopy:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_COPY_AUTHENTICATION_PROMPT);
    case extensions::api::passwords_private::PlaintextReason::kEdit:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EDIT_AUTHENTICATION_PROMPT);
    case extensions::api::passwords_private::PlaintextReason::kNone:
      NOTREACHED();
  }
#else
  return std::u16string();
#endif
}

password_manager::metrics_util::AccessPasswordInSettingsEvent
ConvertPlaintextReason(
    extensions::api::passwords_private::PlaintextReason reason) {
  switch (reason) {
    case extensions::api::passwords_private::PlaintextReason::kCopy:
      return password_manager::metrics_util::ACCESS_PASSWORD_COPIED;
    case extensions::api::passwords_private::PlaintextReason::kView:
      return password_manager::metrics_util::ACCESS_PASSWORD_VIEWED;
    case extensions::api::passwords_private::PlaintextReason::kEdit:
      return password_manager::metrics_util::ACCESS_PASSWORD_EDITED;
    case extensions::api::passwords_private::PlaintextReason::kNone:
      NOTREACHED_IN_MIGRATION();
      return password_manager::metrics_util::ACCESS_PASSWORD_VIEWED;
  }
}

base::flat_set<password_manager::PasswordForm::Store>
ConvertToPasswordFormStores(
    extensions::api::passwords_private::PasswordStoreSet store) {
  switch (store) {
    case extensions::api::passwords_private::PasswordStoreSet::
        kDeviceAndAccount:
      return {password_manager::PasswordForm::Store::kProfileStore,
              password_manager::PasswordForm::Store::kAccountStore};
    case extensions::api::passwords_private::PasswordStoreSet::kDevice:
      return {password_manager::PasswordForm::Store::kProfileStore};
    case extensions::api::passwords_private::PasswordStoreSet::kAccount:
      return {password_manager::PasswordForm::Store::kAccountStore};
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return {};
}

extensions::api::passwords_private::ImportEntry ConvertImportEntry(
    const password_manager::ImportEntry& entry) {
  extensions::api::passwords_private::ImportEntry result;
  result.status =
      static_cast<extensions::api::passwords_private::ImportEntryStatus>(
          entry.status);
  result.url = entry.url;
  result.username = entry.username;
  result.password = entry.password;
  result.id = entry.id;
  return result;
}

// Maps password_manager::ImportResults to
// extensions::api::passwords_private::ImportResults.
extensions::api::passwords_private::ImportResults ConvertImportResults(
    const password_manager::ImportResults& results) {
  base::UmaHistogramEnumeration("PasswordManager.ImportResultsStatus2",
                                results.status);
  extensions::api::passwords_private::ImportResults private_results;
  private_results.status =
      static_cast<extensions::api::passwords_private::ImportResultsStatus>(
          results.status);
  private_results.number_imported = results.number_imported;
  private_results.file_name = results.file_name;
  private_results.displayed_entries.reserve(results.displayed_entries.size());
  for (const auto& entry : results.displayed_entries) {
    private_results.displayed_entries.emplace_back(ConvertImportEntry(entry));
  }
  return private_results;
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)

using password_manager::prefs::kBiometricAuthenticationBeforeFilling;

void ChangeBiometricAuthenticationBeforeFillingSetting(
    PrefService* prefs,
    extensions::PasswordsPrivateDelegate::AuthenticationCallback callback,
    bool success) {
  if (success) {
    prefs->SetBoolean(
        kBiometricAuthenticationBeforeFilling,
        !prefs->GetBoolean(kBiometricAuthenticationBeforeFilling));
  }

  std::move(callback).Run(success);
}

std::u16string GetMessageForBiometricAuthenticationBeforeFillingSetting(
    PrefService* prefs) {
  std::u16string message;
#if BUILDFLAG(IS_MAC)
  const bool pref_enabled =
      prefs->GetBoolean(kBiometricAuthenticationBeforeFilling);
  message = l10n_util::GetStringUTF16(
      pref_enabled ? IDS_PASSWORD_MANAGER_TURN_OFF_FILLING_REAUTH_MAC
                   : IDS_PASSWORD_MANAGER_TURN_ON_FILLING_REAUTH_MAC);
#elif BUILDFLAG(IS_WIN)
  const bool pref_enabled =
      prefs->GetBoolean(kBiometricAuthenticationBeforeFilling);
  message = l10n_util::GetStringUTF16(
      pref_enabled ? IDS_PASSWORD_MANAGER_TURN_OFF_FILLING_REAUTH_WIN
                   : IDS_PASSWORD_MANAGER_TURN_ON_FILLING_REAUTH_WIN);
#elif BUILDFLAG(IS_CHROMEOS)
  const bool pref_enabled =
      prefs->GetBoolean(kBiometricAuthenticationBeforeFilling);
  message = l10n_util::GetStringUTF16(
      pref_enabled ? IDS_PASSWORD_MANAGER_TURN_OFF_FILLING_REAUTH_CHROMEOS
                   : IDS_PASSWORD_MANAGER_TURN_ON_FILLING_REAUTH_CHROMEOS);
#endif
  return message;
}

#endif

void MaybeShowProfileSwitchIPH(Profile* profile) {
#if !BUILDFLAG(IS_CHROMEOS)
  Browser* launched_app = web_app::AppBrowserController::FindForWebApp(
      *profile, web_app::kPasswordManagerAppId);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Try to show promo only if there is profile menu button and there are
  // multiple profiles.
  if (launched_app && launched_app->app_controller() &&
      launched_app->app_controller()->HasProfileMenuButton() &&
      profile_manager && profile_manager->GetNumberOfProfiles() > 1) {
    launched_app->window()->MaybeShowProfileSwitchIPH();
  }
#endif
}

// Returns a passkey model instance if the feature is enabled.
webauthn::PasskeyModel* MaybeGetPasskeyModel(Profile* profile) {
  if (base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)) {
    return PasskeyModelFactory::GetInstance()->GetForProfile(profile);
  }
  return nullptr;
}

std::string GetGroupIconUrl(const password_manager::AffiliatedGroup& group,
                            const syncer::SyncService* sync_service) {
  if (!sync_service) {
    return group.GetFallbackIconURL().spec();
  }

  if (sync_service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    // Users with explicit passphrase should only use fallback icon.
    return group.GetFallbackIconURL().spec();
  }

  // TODO(crbug.com/40067296): Migrate away from `ConsentLevel::kSync` on
  // desktop platforms.
  if (password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
          sync_service)) {
    // Syncing users can use icon provided by the affiliation service.
    return group.GetIconURL().spec();
  }

  for (const CredentialUIEntry& credential : group.GetCredentials()) {
    if (credential.stored_in.contains(
            password_manager::PasswordForm::Store::kAccountStore)) {
      // If at least one credential is stored in the account, icon provided by
      // the affiliation service can be used for the whole group.
      return group.GetIconURL().spec();
    }
  }

  return group.GetFallbackIconURL().spec();
}

}  // namespace

namespace extensions {

PasswordsPrivateDelegateImpl::PasswordsPrivateDelegateImpl(Profile* profile)
    : profile_(profile),
      saved_passwords_presenter_(
          AffiliationServiceFactory::GetForProfile(profile),
          ProfilePasswordStoreFactory::GetForProfile(
              profile,
              ServiceAccessType::EXPLICIT_ACCESS),
          AccountPasswordStoreFactory::GetForProfile(
              profile,
              ServiceAccessType::EXPLICIT_ACCESS),
          MaybeGetPasskeyModel(profile)),
      password_manager_porter_(std::make_unique<PasswordManagerPorter>(
          profile,
          &saved_passwords_presenter_,
          base::BindRepeating(
              &PasswordsPrivateDelegateImpl::OnPasswordsExportProgress,
              base::Unretained(this)))),
      password_check_delegate_(
          profile,
          &saved_passwords_presenter_,
          &credential_id_generator_,
          PasswordsPrivateEventRouterFactory::GetForProfile(profile_)),
      current_entries_initialized_(false) {
  auth_timeout_handler_.Init(
      base::BindRepeating(&PasswordsPrivateDelegateImpl::OsReauthTimeoutCall,
                          weak_ptr_factory_.GetWeakPtr()));
  saved_passwords_presenter_.AddObserver(this);
  saved_passwords_presenter_.Init();

  if (syncer::SyncService* service =
          SyncServiceFactory::GetForProfile(profile_)) {
    sync_service_observation_.Observe(service);
  }

#if !BUILDFLAG(IS_CHROMEOS)
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  install_manager_observation_.Observe(&provider->install_manager());
#endif
}

PasswordsPrivateDelegateImpl::~PasswordsPrivateDelegateImpl() {
  saved_passwords_presenter_.RemoveObserver(this);
  install_manager_observation_.Reset();
#if !BUILDFLAG(IS_WIN)
  if (device_authenticator_) {
    device_authenticator_->Cancel();
  }
#endif
  device_authenticator_.reset();
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<device_reauth::DeviceAuthenticator>
PasswordsPrivateDelegateImpl::GetDeviceAuthenticator(
    content::WebContents* web_contents,
    base::TimeDelta auth_validity_period) {
  if (test_device_authenticator_) {
    return std::move(test_device_authenticator_);
  }

  device_reauth::DeviceAuthParams params(
      auth_validity_period, device_reauth::DeviceAuthSource::kPasswordManager,
      "PasswordManager.ReauthToAccessPasswordInSettings");

  return ChromeDeviceAuthenticatorFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      web_contents->GetTopLevelNativeWindow(), params);
}
#endif

void PasswordsPrivateDelegateImpl::GetSavedPasswordsList(
    UiEntriesCallback callback) {
  if (current_entries_initialized_) {
    std::move(callback).Run(current_entries_);
  } else {
    get_saved_passwords_list_callbacks_.push_back(std::move(callback));
  }
}

PasswordsPrivateDelegate::CredentialsGroups
PasswordsPrivateDelegateImpl::GetCredentialGroups() {
  std::vector<api::passwords_private::CredentialGroup> groups;
  for (const password_manager::AffiliatedGroup& group :
       saved_passwords_presenter_.GetAffiliatedGroups()) {
    api::passwords_private::CredentialGroup group_api;
    group_api.name = group.GetDisplayName();
    group_api.icon_url =
        GetGroupIconUrl(group, SyncServiceFactory::GetForProfile(profile_));

    CHECK(!group.GetCredentials().empty());
    for (const CredentialUIEntry& credential : group.GetCredentials()) {
      group_api.entries.push_back(
          CreatePasswordUiEntryFromCredentialUiEntry(credential));
    }

    groups.push_back(std::move(group_api));
  }
  return groups;
}

void PasswordsPrivateDelegateImpl::GetPasswordExceptionsList(
    ExceptionEntriesCallback callback) {
  if (current_entries_initialized_) {
    std::move(callback).Run(current_exceptions_);
  } else {
    get_password_exception_list_callbacks_.push_back(std::move(callback));
  }
}

std::optional<api::passwords_private::UrlCollection>
PasswordsPrivateDelegateImpl::GetUrlCollection(const std::string& url) {
  GURL url_with_scheme = password_manager_util::ConstructGURLWithScheme(url);
  if (!password_manager::IsValidPasswordURL(url_with_scheme)) {
    return std::nullopt;
  }
  return std::optional<api::passwords_private::UrlCollection>(
      CreateUrlCollectionFromGURL(
          password_manager_util::StripAuthAndParams(url_with_scheme)));
}

bool PasswordsPrivateDelegateImpl::IsAccountStoreDefault(
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  if (!client ||
      !client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    return false;
  }
  return client->GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
         password_manager::PasswordForm::Store::kAccountStore;
}

bool PasswordsPrivateDelegateImpl::AddPassword(
    const std::string& url,
    const std::u16string& username,
    const std::u16string& password,
    const std::u16string& note,
    bool use_account_store,
    content::WebContents* web_contents) {
  password_manager::PasswordForm::Store store_to_use =
      use_account_store ? password_manager::PasswordForm::Store::kAccountStore
                        : password_manager::PasswordForm::Store::kProfileStore;
  CredentialUIEntry credential;

  CredentialFacet facet;
  facet.url = password_manager_util::StripAuthAndParams(
      password_manager_util::ConstructGURLWithScheme(url));
  facet.signon_realm = password_manager::GetSignonRealm(facet.url);
  credential.facets.push_back(std::move(facet));
  credential.username = username;
  credential.password = password;
  credential.note = note;
  credential.stored_in = {store_to_use};
  return saved_passwords_presenter_.AddCredential(credential);
}

bool PasswordsPrivateDelegateImpl::ChangeCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  const CredentialUIEntry* original_credential =
      credential_id_generator_.TryGetKey(credential.id);
  if (!original_credential) {
    return false;
  }
  CredentialUIEntry updated_credential = *original_credential;
  updated_credential.username = base::UTF8ToUTF16(credential.username);
  if (credential.password) {
    updated_credential.password = base::UTF8ToUTF16(*credential.password);
  }
  if (credential.note) {
    updated_credential.note = base::UTF8ToUTF16(*credential.note);
  }
  if (credential.display_name) {
    CHECK(!updated_credential.passkey_credential_id.empty());
    updated_credential.user_display_name =
        base::UTF8ToUTF16(*credential.display_name);
  }
  switch (saved_passwords_presenter_.EditSavedCredentials(*original_credential,
                                                          updated_credential)) {
    case password_manager::SavedPasswordsPresenter::EditResult::kSuccess:
    case password_manager::SavedPasswordsPresenter::EditResult::kNothingChanged:
      return true;
    case password_manager::SavedPasswordsPresenter::EditResult::kNotFound:
    case password_manager::SavedPasswordsPresenter::EditResult::kAlreadyExisits:
    case password_manager::SavedPasswordsPresenter::EditResult::kEmptyPassword:
      return false;
  }
}

void PasswordsPrivateDelegateImpl::RemoveCredential(
    int id,
    api::passwords_private::PasswordStoreSet from_stores) {
  const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
  if (!entry) {
    return;
  }

  CredentialUIEntry copy = *entry;
  copy.stored_in = ConvertToPasswordFormStores(from_stores);

  saved_passwords_presenter_.RemoveCredential(copy);

  // Record that a password removal action happened.
  if (copy.stored_in.contains(
          password_manager::PasswordForm::Store::kAccountStore)) {
    AddPasswordRemovalReason(
        profile_->GetPrefs(), password_manager::IsAccountStore(true),
        password_manager::metrics_util::PasswordManagerCredentialRemovalReason::
            kSettings);
  }
  if (copy.stored_in.contains(
          password_manager::PasswordForm::Store::kProfileStore)) {
    AddPasswordRemovalReason(
        profile_->GetPrefs(), password_manager::IsAccountStore(false),
        password_manager::metrics_util::PasswordManagerCredentialRemovalReason::
            kSettings);
  }

  if (entry->blocked_by_user) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemovePasswordException"));
  } else if (!entry->passkey_credential_id.empty()) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemovePasskey"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
  }
}

void PasswordsPrivateDelegateImpl::RemovePasswordException(int id) {
  RemoveCredential(id,
                   api::passwords_private::PasswordStoreSet::kDeviceAndAccount);
}

void PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrException() {
  saved_passwords_presenter_.UndoLastRemoval();
}

void PasswordsPrivateDelegateImpl::RequestPlaintextPassword(
    int id,
    api::passwords_private::PlaintextReason reason,
    PlaintextPasswordCallback callback,
    content::WebContents* web_contents) {
  AuthenticateUser(
      web_contents, kPasswordManagerAuthValidity, GetReauthPurpose(reason),
      base::BindOnce(
          &PasswordsPrivateDelegateImpl::OnRequestPlaintextPasswordAuthResult,
          weak_ptr_factory_.GetWeakPtr(), id, reason, std::move(callback)));
}

void PasswordsPrivateDelegateImpl::RequestCredentialsDetails(
    const std::vector<int>& ids,
    UiEntriesCallback callback,
    content::WebContents* web_contents) {
  AuthenticateUser(
      web_contents, kPasswordManagerAuthValidity,
      GetReauthPurpose(api::passwords_private::PlaintextReason::kView),
      base::BindOnce(
          &PasswordsPrivateDelegateImpl::OnRequestCredentialDetailsAuthResult,
          weak_ptr_factory_.GetWeakPtr(), ids, std::move(callback),
          web_contents->GetWeakPtr()));
}

void PasswordsPrivateDelegateImpl::OnFetchingFamilyMembersCompleted(
    FetchFamilyResultsCallback callback,
    std::vector<password_manager::RecipientInfo> family_members,
    FetchFamilyMembersRequestStatus request_status) {
  api::passwords_private::FamilyFetchResults results;
  switch (request_status) {
    case FetchFamilyMembersRequestStatus::kUnknown:
    case FetchFamilyMembersRequestStatus::kNetworkError:
    case FetchFamilyMembersRequestStatus::kPendingRequest:
      results.status = api::passwords_private::FamilyFetchStatus::kUnknownError;
      break;
    case FetchFamilyMembersRequestStatus::kSuccess:
    case FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers:
      // TODO(crbug.com/40268194): Add new FamilyFetchStatus and its handling.
      results.status = api::passwords_private::FamilyFetchStatus::kSuccess;
      break;
    case FetchFamilyMembersRequestStatus::kNoFamily:
      results.status = api::passwords_private::FamilyFetchStatus::kNoMembers;
  }
  if (request_status == FetchFamilyMembersRequestStatus::kSuccess) {
    for (const password_manager::RecipientInfo& family_member :
         family_members) {
      api::passwords_private::RecipientInfo recipient_info;
      recipient_info.user_id = family_member.user_id;
      recipient_info.email = family_member.email;
      recipient_info.display_name = family_member.user_name;
      recipient_info.profile_image_url = family_member.profile_image_url;

      if (!family_member.public_key.key.empty()) {
        recipient_info.is_eligible = true;
        api::passwords_private::PublicKey public_key;
        public_key.value = family_member.public_key.key;
        public_key.version = family_member.public_key.key_version;
        recipient_info.public_key = std::move(public_key);
      }

      results.family_members.push_back(std::move(recipient_info));
    }
  }
  std::move(callback).Run(results);
}

void PasswordsPrivateDelegateImpl::OsReauthTimeoutCall() {
#if !BUILDFLAG(IS_LINUX)
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnPasswordManagerAuthTimeout();
  }
#endif
}

void PasswordsPrivateDelegateImpl::SetCredentials(
    std::vector<CredentialUIEntry> credentials) {
  // Create lists of PasswordUiEntry and ExceptionEntry objects to send to
  // observers.
  current_entries_.clear();
  current_exceptions_.clear();

  for (CredentialUIEntry& credential : credentials) {
    if (credential.blocked_by_user) {
      api::passwords_private::ExceptionEntry current_exception_entry;
      current_exception_entry.urls =
          CreateUrlCollectionFromCredential(credential);
      current_exception_entry.id =
          credential_id_generator_.GenerateId(std::move(credential));
      current_exceptions_.push_back(std::move(current_exception_entry));
    } else {
      current_entries_.push_back(
          CreatePasswordUiEntryFromCredentialUiEntry(std::move(credential)));
    }
  }
  for (CredentialUIEntry& credential :
       saved_passwords_presenter_.GetBlockedSites()) {
    api::passwords_private::ExceptionEntry current_exception_entry;
    current_exception_entry.urls =
        CreateUrlCollectionFromCredential(credential);
    current_exception_entry.id =
        credential_id_generator_.GenerateId(std::move(credential));
    current_exceptions_.push_back(std::move(current_exception_entry));
  }

  if (current_entries_initialized_) {
    CHECK(get_saved_passwords_list_callbacks_.empty());
    CHECK(get_password_exception_list_callbacks_.empty());
  }

  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnSavedPasswordsListChanged(current_entries_);
    router->OnPasswordExceptionsListChanged(current_exceptions_);
  }

  current_entries_initialized_ = true;

  for (auto& callback : get_saved_passwords_list_callbacks_) {
    std::move(callback).Run(current_entries_);
  }
  get_saved_passwords_list_callbacks_.clear();
  for (auto& callback : get_password_exception_list_callbacks_) {
    std::move(callback).Run(current_exceptions_);
  }
  get_password_exception_list_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::MovePasswordsToAccount(
    const std::vector<int>& ids,
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);

  if (!client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    return;
  }

  std::vector<CredentialUIEntry> credentials_to_move;
  credentials_to_move.reserve(ids.size());
  for (int id : ids) {
    const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
    if (!entry) {
      continue;
    }
    credentials_to_move.push_back(*entry);
  }

  saved_passwords_presenter_.MoveCredentialsToAccount(
      credentials_to_move,
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kExplicitlyTriggeredInSettings);
}

void PasswordsPrivateDelegateImpl::FetchFamilyMembers(
    FetchFamilyResultsCallback callback) {
  if (!sharing_password_recipients_fetcher_) {
    sharing_password_recipients_fetcher_ =
        std::make_unique<password_manager::RecipientsFetcherImpl>(
            chrome::GetChannel(),
            profile_->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess(),
            IdentityManagerFactory::GetForProfile(profile_));
  }
  sharing_password_recipients_fetcher_->FetchFamilyMembers(base::BindOnce(
      &PasswordsPrivateDelegateImpl::OnFetchingFamilyMembersCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PasswordsPrivateDelegateImpl::SharePassword(
    int id,
    const ShareRecipients& recipients) {
  const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
  if (!entry) {
    return;
  }

  std::vector<password_manager::PasswordForm> corresponding_credentials =
      saved_passwords_presenter_.GetCorrespondingPasswordForms(*entry);
  if (corresponding_credentials.empty()) {
    return;
  }

  password_manager::PasswordSenderService* password_sender_service =
      PasswordSenderServiceFactory::GetForProfile(profile_);
  for (const api::passwords_private::RecipientInfo& recipient_info :
       recipients) {
    CHECK(recipient_info.public_key.has_value());
    password_manager::PublicKey public_key;
    public_key.key = recipient_info.public_key.value().value;
    public_key.key_version = recipient_info.public_key.value().version;
    password_sender_service->SendPasswords(
        corresponding_credentials, {.user_id = recipient_info.user_id,
                                    .public_key = std::move(public_key)});
  }
}

void PasswordsPrivateDelegateImpl::ImportPasswords(
    api::passwords_private::PasswordStoreSet to_store,
    ImportResultsCallback results_callback,
    content::WebContents* web_contents) {
  DCHECK_NE(api::passwords_private::PasswordStoreSet::kDeviceAndAccount,
            to_store);
  password_manager::PasswordForm::Store store_to_use =
      *ConvertToPasswordFormStores(to_store).begin();
  password_manager_porter_->Import(
      web_contents, store_to_use,
      base::BindOnce(&ConvertImportResults).Then(std::move(results_callback)));
}

void PasswordsPrivateDelegateImpl::ContinueImport(
    const std::vector<int>& selected_ids,
    ImportResultsCallback results_callback,
    content::WebContents* web_contents) {
  if (selected_ids.empty()) {
    password_manager_porter_->ContinueImport(
        selected_ids, base::BindOnce(&ConvertImportResults)
                          .Then(std::move(results_callback)));
    return;
  }

  std::u16string message;
#if BUILDFLAG(IS_MAC)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_IMPORT_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
#elif BUILDFLAG(IS_WIN)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_IMPORT_AUTHENTICATION_PROMPT);
#endif

  AuthenticateUser(
      web_contents, base::Seconds(0), message,
      base::BindOnce(&PasswordsPrivateDelegateImpl::OnImportPasswordsAuthResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(results_callback), selected_ids));
}

void PasswordsPrivateDelegateImpl::ResetImporter(bool delete_file) {
  password_manager_porter_->ResetImporter(delete_file);
}

void PasswordsPrivateDelegateImpl::ExportPasswords(
    base::OnceCallback<void(const std::string&)> accepted_callback,
    content::WebContents* web_contents) {
  std::u16string message;
#if BUILDFLAG(IS_MAC)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_EXPORT_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
#elif BUILDFLAG(IS_WIN)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_EXPORT_AUTHENTICATION_PROMPT);
#endif

  AuthenticateUser(
      web_contents, base::Seconds(0), message,
      base::BindOnce(&PasswordsPrivateDelegateImpl::OnExportPasswordsAuthResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(accepted_callback), web_contents->GetWeakPtr()));
}

api::passwords_private::ExportProgressStatus
PasswordsPrivateDelegateImpl::GetExportProgressStatus() {
  return ConvertStatus(password_manager_porter_->GetExportProgressStatus());
}

bool PasswordsPrivateDelegateImpl::IsAccountStorageEnabled() {
  return password_manager::features_util::IsOptedInForAccountStorage(
      profile_->GetPrefs(), SyncServiceFactory::GetForProfile(profile_));
}

void PasswordsPrivateDelegateImpl::SetAccountStorageEnabled(
    bool enabled,
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  if (enabled ==
      client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    return;
  }
  if (!enabled) {
    client->GetPasswordFeatureManager()->OptOutOfAccountStorage();
    return;
  }

  if (!password_manager::features_util::AreAccountStorageOptInPromosAllowed()) {
    // No need to show a reauth dialog in this case, just enable directly.
    client->GetPasswordFeatureManager()->OptInToAccountStorage();
    return;
  }

  // The enabled pref is automatically set upon successful reauth.
  client->TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint::kPasswordSettings, base::DoNothing());
}

std::vector<api::passwords_private::PasswordUiEntry>
PasswordsPrivateDelegateImpl::GetInsecureCredentials() {
  return password_check_delegate_.GetInsecureCredentials();
}

std::vector<api::passwords_private::PasswordUiEntryList>
PasswordsPrivateDelegateImpl::GetCredentialsWithReusedPassword() {
  return password_check_delegate_.GetCredentialsWithReusedPassword();
}

bool PasswordsPrivateDelegateImpl::MuteInsecureCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  return password_check_delegate_.MuteInsecureCredential(credential);
}

bool PasswordsPrivateDelegateImpl::UnmuteInsecureCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  return password_check_delegate_.UnmuteInsecureCredential(credential);
}

void PasswordsPrivateDelegateImpl::StartPasswordCheck(
    StartPasswordCheckCallback callback) {
  password_check_delegate_.StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(callback));
  auto* sentiment_service =
      TrustSafetySentimentServiceFactory::GetForProfile(profile_);
  if (!sentiment_service) {
    return;
  }
  sentiment_service->RanPasswordCheck();
}

api::passwords_private::PasswordCheckStatus
PasswordsPrivateDelegateImpl::GetPasswordCheckStatus() {
  return password_check_delegate_.GetPasswordCheckStatus();
}

void PasswordsPrivateDelegateImpl::SwitchBiometricAuthBeforeFillingState(

    content::WebContents* web_contents,
    AuthenticationCallback authentication_callback) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  AuthResultCallback callback =
      base::BindOnce(&ChangeBiometricAuthenticationBeforeFillingSetting,
                     profile_->GetPrefs(), std::move(authentication_callback));

  AuthenticateUser(web_contents, base::Seconds(0),
                   GetMessageForBiometricAuthenticationBeforeFillingSetting(
                       profile_->GetPrefs()),
                   std::move(callback));
#else
  NOTIMPLEMENTED();
#endif
}

void PasswordsPrivateDelegateImpl::ShowAddShortcutDialog(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  DCHECK(browser);
  web_app::CreateWebAppFromCurrentWebContents(
      browser, web_app::WebAppInstallFlow::kInstallSite);
  base::UmaHistogramEnumeration(
      "PasswordManager.ShortcutMetric",
      password_manager::metrics_util::PasswordManagerShortcutMetric::
          kAddShortcutClicked);
}

void PasswordsPrivateDelegateImpl::ShowExportedFileInShell(
    content::WebContents* web_contents,
    std::string file_path) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  DCHECK(browser);
#if !BUILDFLAG(IS_WIN)
  base::FilePath path(file_path);
#else
  base::FilePath path(base::UTF8ToWide(file_path));
#endif
  platform_util::ShowItemInFolder(browser->profile(), path);
}

void PasswordsPrivateDelegateImpl::ChangePasswordManagerPin(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> success_callback) {
  ChangePinController* controller =
      ChangePinController::ForWebContents(web_contents);
  if (controller) {
    controller->StartChangePin(std::move(success_callback));
  }
}

void PasswordsPrivateDelegateImpl::IsPasswordManagerPinAvailable(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> pin_available_callback) {
  ChangePinController* controller =
      ChangePinController::ForWebContents(web_contents);
  if (!controller) {
    std::move(pin_available_callback).Run(false);
    return;
  }
  controller->IsChangePinFlowAvailable(std::move(pin_available_callback));
}

void PasswordsPrivateDelegateImpl::DisconnectCloudAuthenticator(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> success_callback) {
  EnclaveManagerInterface* enclave_manager =
      EnclaveManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (enclave_manager) {
    enclave_manager->Unenroll(std::move(success_callback));
  }
}

bool PasswordsPrivateDelegateImpl::IsConnectedToCloudAuthenticator(
    content::WebContents* web_contents) {
  EnclaveManagerInterface* enclave_manager =
      EnclaveManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  if (!enclave_manager) {
    return false;
  }

  return enclave_manager->is_registered();
}

void PasswordsPrivateDelegateImpl::DeleteAllPasswordManagerData(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> success_callback) {
  std::u16string message;
#if BUILDFLAG(IS_MAC)
  message = l10n_util::GetStringFUTF16(
      IDS_PASSWORDS_PAGE_DELETE_ALL_DATA_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX,
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE));
#elif BUILDFLAG(IS_WIN)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_DELETE_ALL_DATA_AUTHENTICATION_PROMPT);
#endif

  AuthenticateUser(
      web_contents, base::Seconds(0), message,
      base::BindOnce(&PasswordsPrivateDelegateImpl::OnDeleteAllDataAuthResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback)));
}

void PasswordsPrivateDelegateImpl::OnDeleteAllDataAuthResult(
    base::OnceCallback<void(bool)> success_callback,
    bool authenticated) {
  if (!authenticated) {
    std::move(success_callback).Run(false);
    return;
  }

  saved_passwords_presenter_.DeleteAllData(std::move(success_callback));

  // Record password removal from both stores. "Delete all" requires UI
  // confirmation and re-authentication, indicating strong user intent to
  // remove all password data.
  AddPasswordRemovalReason(
      profile_->GetPrefs(), password_manager::IsAccountStore(true),
      password_manager::metrics_util::PasswordManagerCredentialRemovalReason::
          kDeleteAllPasswordManagerData);
  AddPasswordRemovalReason(
      profile_->GetPrefs(), password_manager::IsAccountStore(false),
      password_manager::metrics_util::PasswordManagerCredentialRemovalReason::
          kDeleteAllPasswordManagerData);
}

base::WeakPtr<PasswordsPrivateDelegate>
PasswordsPrivateDelegateImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

password_manager::InsecureCredentialsManager*
PasswordsPrivateDelegateImpl::GetInsecureCredentialsManager() {
  return password_check_delegate_.GetInsecureCredentialsManager();
}

void PasswordsPrivateDelegateImpl::RestartAuthTimer() {
  auth_timeout_handler_.RestartAuthTimer();
}

void PasswordsPrivateDelegateImpl::MaybeShowPasswordShareButtonIPH(
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }
  Browser* browser = chrome::FindBrowserWithTab(web_contents.get());
  if (!browser || !browser->window()) {
    return;
  }
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHPasswordSharingFeature);
}

void PasswordsPrivateDelegateImpl::OnPasswordsExportProgress(
    const password_manager::PasswordExportInfo& progress) {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnPasswordsExportProgress(ConvertStatus(progress.status),
                                      progress.file_path, progress.folder_name);
  }
}

void PasswordsPrivateDelegateImpl::OnRequestPlaintextPasswordAuthResult(
    int id,
    api::passwords_private::PlaintextReason reason,
    PlaintextPasswordCallback callback,
    bool authenticated) {
  if (!authenticated) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
  if (!entry) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (reason == api::passwords_private::PlaintextReason::kCopy) {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(entry->password);
    clipboard_writer.MarkAsConfidential();
    // In case of copy we don't need to give password back to UI. callback
    // will receive either empty string in case of success or null otherwise.
    // Copying occurs here so javascript doesn't need plaintext password.
    std::move(callback).Run(std::u16string());
  } else {
    std::move(callback).Run(entry->password);
  }
  EmitHistogramsForCredentialAccess(*entry, reason);
}

void PasswordsPrivateDelegateImpl::OnRequestCredentialDetailsAuthResult(
    const std::vector<int>& ids,
    UiEntriesCallback callback,
    base::WeakPtr<content::WebContents> web_contents,
    bool authenticated) {
  if (!authenticated || !web_contents) {
    std::move(callback).Run({});
    return;
  }

  CredentialUIEntry last_entry;
  std::vector<api::passwords_private::PasswordUiEntry> passwords;
  for (int id : ids) {
    const CredentialUIEntry* credential =
        credential_id_generator_.TryGetKey(id);
    if (!credential) {
      continue;
    }

    api::passwords_private::PasswordUiEntry password_ui_entry =
        CreatePasswordUiEntryFromCredentialUiEntry(*credential);
    password_ui_entry.password = base::UTF16ToUTF8(credential->password);
    password_ui_entry.note = base::UTF16ToUTF8(credential->note);
    // password_manager::MovePasswordsToAccountStore() takes care of moving the
    // entire equivalence class, so passing the first element is fine.
    passwords.push_back(std::move(password_ui_entry));

    last_entry = *credential;
  }

  if (!passwords.empty()) {
    EmitHistogramsForCredentialAccess(
        last_entry, api::passwords_private::PlaintextReason::kView);
  }
  std::move(callback).Run(std::move(passwords));

  // Attempt to show "Password Share Button" help-bubble when the user opens
  // PasswordsDetailsSection. The task is posted with a delay because WebUI is
  // rendered asynchronously and help-bubble anchor might be registered with
  // some delay.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &PasswordsPrivateDelegateImpl::MaybeShowPasswordShareButtonIPH,
          weak_ptr_factory_.GetWeakPtr(), web_contents),
      base::Milliseconds(300));
}

void PasswordsPrivateDelegateImpl::OnExportPasswordsAuthResult(
    base::OnceCallback<void(const std::string&)> accepted_callback,
    base::WeakPtr<content::WebContents> web_contents,
    bool authenticated) {
  if (!authenticated || !web_contents) {
    std::move(accepted_callback).Run(kReauthenticationFailed);
    return;
  }

  bool accepted = password_manager_porter_->Export(web_contents);
  std::move(accepted_callback)
      .Run(accepted ? std::string() : kExportInProgress);
}

void PasswordsPrivateDelegateImpl::OnImportPasswordsAuthResult(
    ImportResultsCallback results_callback,
    const std::vector<int>& selected_ids,
    bool authenticated) {
  if (!authenticated) {
    password_manager::ImportResults result;
    result.status = password_manager::ImportResults::DISMISSED;
    std::move(results_callback).Run(ConvertImportResults(result));
    return;
  }

  CHECK(password_manager_porter_);
  password_manager_porter_->ContinueImport(
      selected_ids,
      base::BindOnce(&ConvertImportResults).Then(std::move(results_callback)));
}

void PasswordsPrivateDelegateImpl::OnStateChanged(
    syncer::SyncService* sync_service) {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnAccountStorageEnabledStateChanged(IsAccountStorageEnabled());
  }
}

void PasswordsPrivateDelegateImpl::OnSyncShutdown(syncer::SyncService* sync) {
  sync_service_observation_.Reset();
}

void PasswordsPrivateDelegateImpl::OnReauthCompleted(bool authenticated) {
  device_authenticator_.reset();

  auth_timeout_handler_.OnUserReauthenticationResult(authenticated);
}

void PasswordsPrivateDelegateImpl::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  SetCredentials(saved_passwords_presenter_.GetSavedCredentials());
}

void PasswordsPrivateDelegateImpl::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  if (app_id != web_app::kPasswordManagerAppId) {
    return;
  }
  // Post task with delay because new browser window for an app isn't created
  // yet.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&MaybeShowProfileSwitchIPH, profile_),
      base::Seconds(1));
  base::UmaHistogramEnumeration(
      "PasswordManager.ShortcutMetric",
      password_manager::metrics_util::PasswordManagerShortcutMetric::
          kShortcutInstalled);
}

void PasswordsPrivateDelegateImpl::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void PasswordsPrivateDelegateImpl::EmitHistogramsForCredentialAccess(
    const CredentialUIEntry& entry,
    api::passwords_private::PlaintextReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccessPasswordInSettings",
      ConvertPlaintextReason(reason),
      password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
}

void PasswordsPrivateDelegateImpl::AuthenticateUser(
    content::WebContents* web_contents,
    base::TimeDelta auth_validity_period,
    const std::u16string& message,
    AuthResultCallback auth_callback) {
  auto callback = password_manager::metrics_util::TimeCallbackMediumTimes(
      std::move(auth_callback), "PasswordManager.Settings.AuthenticationTime2");

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
  std::move(callback).Run(true);
#else
  CHECK(web_contents);

  // Authentication on Windows cannot be canceled.
  // TODO(crbug.com/40241199): Remove Cancel and instead simply destroy
  // |device_authenticator_|.
  if (device_authenticator_) {
#if BUILDFLAG(IS_WIN)
    // `device_authenticator_` lives as long as the authentication is in
    // progress. Since there is currently no way of canceling authentication
    // if the new one wants to start, new authentications will be resolved as if
    // they failed until the pending authentication gets resolved by the user.
    std::move(callback).Run(false);
    return;
#else
    device_authenticator_->Cancel();
#endif
  }
  device_authenticator_ =
      GetDeviceAuthenticator(web_contents, auth_validity_period);

  AuthResultCallback on_reauth_completed =
      base::BindOnce(&PasswordsPrivateDelegateImpl::OnReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr());

  auto pass_through = base::BindOnce(
      [](AuthResultCallback callback, bool auth_result) {
        std::move(callback).Run(auth_result);
        return auth_result;
      },
      std::move(callback));

  device_authenticator_->AuthenticateWithMessage(
      message, std::move(pass_through).Then(std::move(on_reauth_completed)));
#endif
}

api::passwords_private::PasswordUiEntry
PasswordsPrivateDelegateImpl::CreatePasswordUiEntryFromCredentialUiEntry(
    CredentialUIEntry credential) {
  api::passwords_private::PasswordUiEntry entry;
  base::ranges::transform(credential.GetAffiliatedDomains(),
                          std::back_inserter(entry.affiliated_domains),
                          [](const CredentialUIEntry::DomainInfo& domain) {
                            api::passwords_private::DomainInfo domain_info;
                            // `domain.name` is used to redirect to the Password
                            // Manager page for the password represented by the
                            // current `CredentialUIEntry`.
                            // LINT.IfChange
                            domain_info.name = domain.name;
                            // LINT.ThenChange(//chrome/browser/ui/passwords/bubble_controllers/manage_passwords_bubble_controller.cc)
                            domain_info.url = domain.url.spec();
                            domain_info.signon_realm = domain.signon_realm;
                            return domain_info;
                          });
  entry.is_passkey = !credential.passkey_credential_id.empty();
  entry.username = base::UTF16ToUTF8(credential.username);
  if (entry.is_passkey) {
    entry.display_name = base::UTF16ToUTF8(credential.user_display_name);
  }
  if (credential.creation_time.has_value()) {
    entry.creation_time =
        credential.creation_time->InMillisecondsSinceUnixEpoch();
  }
  entry.stored_in = extensions::StoreSetFromCredential(credential);
  if (credential.federation_origin.IsValid()) {
    std::u16string formatted_origin =
        url_formatter::FormatUrlForSecurityDisplay(
            credential.federation_origin.GetURL(),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

    entry.federation_text = base::UTF16ToUTF8(formatted_origin);
  }
  std::optional<GURL> change_password_url = credential.GetChangePasswordURL();
  if (change_password_url.has_value()) {
    entry.change_password_url = change_password_url->spec();
  }
  entry.id = credential_id_generator_.GenerateId(std::move(credential));
  return entry;
}

}  // namespace extensions
