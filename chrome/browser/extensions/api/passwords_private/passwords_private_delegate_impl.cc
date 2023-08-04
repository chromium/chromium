// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <memory>
#include <utility>

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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
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
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/sharing/recipients_fetcher_impl.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils_chromeos.h"
#endif

namespace {

using password_manager::CredentialFacet;
using password_manager::CredentialUIEntry;
using password_manager::FetchFamilyMembersRequestStatus;

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
    case password_manager::ExportProgressStatus::NOT_STARTED:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_NOT_STARTED;
    case password_manager::ExportProgressStatus::IN_PROGRESS:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_IN_PROGRESS;
    case password_manager::ExportProgressStatus::SUCCEEDED:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_SUCCEEDED;
    case password_manager::ExportProgressStatus::FAILED_CANCELLED:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_FAILED_CANCELLED;
    case password_manager::ExportProgressStatus::FAILED_WRITE_FAILED:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_FAILED_WRITE_FAILED;
  }

  NOTREACHED();
  return extensions::api::passwords_private::ExportProgressStatus::
      EXPORT_PROGRESS_STATUS_NONE;
}

password_manager::ReauthPurpose GetReauthPurpose(
    extensions::api::passwords_private::PlaintextReason reason) {
  switch (reason) {
    case extensions::api::passwords_private::PLAINTEXT_REASON_VIEW:
      return password_manager::ReauthPurpose::VIEW_PASSWORD;
    case extensions::api::passwords_private::PLAINTEXT_REASON_COPY:
      return password_manager::ReauthPurpose::COPY_PASSWORD;
    case extensions::api::passwords_private::PLAINTEXT_REASON_EDIT:
      return password_manager::ReauthPurpose::EDIT_PASSWORD;
    case extensions::api::passwords_private::PLAINTEXT_REASON_NONE:
      break;
  }

  NOTREACHED();
  return password_manager::ReauthPurpose::VIEW_PASSWORD;
}

password_manager::metrics_util::AccessPasswordInSettingsEvent
ConvertPlaintextReason(
    extensions::api::passwords_private::PlaintextReason reason) {
  switch (reason) {
    case extensions::api::passwords_private::PLAINTEXT_REASON_COPY:
      return password_manager::metrics_util::ACCESS_PASSWORD_COPIED;
    case extensions::api::passwords_private::PLAINTEXT_REASON_VIEW:
      return password_manager::metrics_util::ACCESS_PASSWORD_VIEWED;
    case extensions::api::passwords_private::PLAINTEXT_REASON_EDIT:
      return password_manager::metrics_util::ACCESS_PASSWORD_EDITED;
    case extensions::api::passwords_private::PLAINTEXT_REASON_NONE:
      NOTREACHED();
      return password_manager::metrics_util::ACCESS_PASSWORD_VIEWED;
  }
}

base::flat_set<password_manager::PasswordForm::Store>
ConvertToPasswordFormStores(
    extensions::api::passwords_private::PasswordStoreSet store) {
  switch (store) {
    case extensions::api::passwords_private::
        PASSWORD_STORE_SET_DEVICE_AND_ACCOUNT:
      return {password_manager::PasswordForm::Store::kProfileStore,
              password_manager::PasswordForm::Store::kAccountStore};
    case extensions::api::passwords_private::PASSWORD_STORE_SET_DEVICE:
      return {password_manager::PasswordForm::Store::kProfileStore};
    case extensions::api::passwords_private::PASSWORD_STORE_SET_ACCOUNT:
      return {password_manager::PasswordForm::Store::kAccountStore};
    default:
      break;
  }
  NOTREACHED();
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
scoped_refptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator(
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  return client->GetDeviceAuthenticator();
}
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

using password_manager::prefs::kBiometricAuthenticationBeforeFilling;

void ChangeBiometricAuthenticationBeforeFillingSetting(PrefService* prefs,
                                                       bool success) {
  if (success) {
    prefs->SetBoolean(
        kBiometricAuthenticationBeforeFilling,
        !prefs->GetBoolean(kBiometricAuthenticationBeforeFilling));
  }
}

std::u16string GetMessageForBiometricAuthenticationBeforeFillingSetting(
    PrefService* prefs) {
  const bool pref_enabled =
      prefs->GetBoolean(kBiometricAuthenticationBeforeFilling);
#if BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(
      pref_enabled ? IDS_PASSWORD_MANAGER_TURN_OFF_FILLING_REAUTH_MAC
                   : IDS_PASSWORD_MANAGER_TURN_ON_FILLING_REAUTH_MAC);
#elif BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(
      pref_enabled ? IDS_PASSWORD_MANAGER_TURN_OFF_FILLING_REAUTH_WIN
                   : IDS_PASSWORD_MANAGER_TURN_ON_FILLING_REAUTH_WIN);
#endif
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
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManagerPasskeys) &&
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)) {
    return PasskeyModelFactory::GetInstance()->GetForProfile(profile);
  }
  return nullptr;
}

}  // namespace

namespace extensions {

PasswordsPrivateDelegateImpl::PasswordsPrivateDelegateImpl(Profile* profile)
    : profile_(profile),
      saved_passwords_presenter_(
          AffiliationServiceFactory::GetForProfile(profile),
          PasswordStoreFactory::GetForProfile(
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
      password_account_storage_settings_watcher_(
          std::make_unique<
              password_manager::PasswordAccountStorageSettingsWatcher>(
              profile_->GetPrefs(),
              SyncServiceFactory::GetForProfile(profile_),
              base::BindRepeating(&PasswordsPrivateDelegateImpl::
                                      OnAccountStorageOptInStateChanged,
                                  base::Unretained(this)))),
      password_check_delegate_(profile,
                               &saved_passwords_presenter_,
                               &credential_id_generator_),
      current_entries_initialized_(false),
      is_initialized_(false),
      web_contents_(nullptr) {
  password_access_authenticator_.Init(
      base::BindRepeating(&PasswordsPrivateDelegateImpl::OsReauthCall,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PasswordsPrivateDelegateImpl::OsReauthTimeoutCall,
                          weak_ptr_factory_.GetWeakPtr()));
  saved_passwords_presenter_.AddObserver(this);
  saved_passwords_presenter_.Init();

#if !BUILDFLAG(IS_CHROMEOS)
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  install_manager_observation_.Observe(&provider->install_manager());
#endif
}

PasswordsPrivateDelegateImpl::~PasswordsPrivateDelegateImpl() {
  saved_passwords_presenter_.RemoveObserver(this);
  install_manager_observation_.Reset();
}

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
  bool sync_enabled = password_manager::sync_util::IsPasswordSyncEnabled(
      SyncServiceFactory::GetForProfile(profile_));
  for (const password_manager::AffiliatedGroup& group :
       saved_passwords_presenter_.GetAffiliatedGroups()) {
    api::passwords_private::CredentialGroup group_api;
    group_api.name = group.GetDisplayName();
    group_api.icon_url = sync_enabled ? group.GetIconURL().spec()
                                      : group.GetFallbackIconURL().spec();

    DCHECK(!group.GetCredentials().empty());
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

absl::optional<api::passwords_private::UrlCollection>
PasswordsPrivateDelegateImpl::GetUrlCollection(const std::string& url) {
  GURL url_with_scheme = password_manager_util::ConstructGURLWithScheme(url);
  if (!password_manager_util::IsValidPasswordURL(url_with_scheme)) {
    return absl::nullopt;
  }
  return absl::optional<api::passwords_private::UrlCollection>(
      CreateUrlCollectionFromGURL(
          password_manager_util::StripAuthAndParams(url_with_scheme)));
}

bool PasswordsPrivateDelegateImpl::IsAccountStoreDefault(
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  DCHECK(client->GetPasswordFeatureManager()->IsOptedInForAccountStorage());
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
  bool success = saved_passwords_presenter_.AddCredential(credential);

  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  // Update the default store to the last used one.
  if (success &&
      client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    client->GetPasswordFeatureManager()->SetDefaultPasswordStore(store_to_use);
  }
  return success;
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
  ExecuteFunction(
      base::BindOnce(&PasswordsPrivateDelegateImpl::RemoveEntryInternal,
                     base::Unretained(this), id, from_stores));
}

void PasswordsPrivateDelegateImpl::RemoveEntryInternal(
    int id,
    api::passwords_private::PasswordStoreSet from_stores) {
  const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
  if (!entry) {
    return;
  }

  CredentialUIEntry copy = *entry;
  copy.stored_in = ConvertToPasswordFormStores(from_stores);

  saved_passwords_presenter_.RemoveCredential(copy);

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
  ExecuteFunction(base::BindOnce(
      &PasswordsPrivateDelegateImpl::RemoveEntryInternal,
      base::Unretained(this), id,
      api::passwords_private::PASSWORD_STORE_SET_DEVICE_AND_ACCOUNT));
}

void PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrException() {
  ExecuteFunction(base::BindOnce(
      &PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrExceptionInternal,
      base::Unretained(this)));
}

void PasswordsPrivateDelegateImpl::
    UndoRemoveSavedPasswordOrExceptionInternal() {
  saved_passwords_presenter_.UndoLastRemoval();
}

void PasswordsPrivateDelegateImpl::RequestPlaintextPassword(
    int id,
    api::passwords_private::PlaintextReason reason,
    PlaintextPasswordCallback callback,
    content::WebContents* web_contents) {
  // Save |web_contents| so that it can be used later when OsReauthCall() is
  // called. Note: This is safe because the |web_contents| is used before
  // exiting this method.
  // TODO(crbug.com/495290): Pass the native window directly to the
  // reauth-handling code.
  web_contents_ = web_contents;
  password_access_authenticator_.EnsureUserIsAuthenticated(
      GetReauthPurpose(reason),
      base::BindOnce(
          &PasswordsPrivateDelegateImpl::OnRequestPlaintextPasswordAuthResult,
          weak_ptr_factory_.GetWeakPtr(), id, reason, std::move(callback)));
}

void PasswordsPrivateDelegateImpl::RequestCredentialsDetails(
    const std::vector<int>& ids,
    UiEntriesCallback callback,
    content::WebContents* web_contents) {
  // Save |web_contents| so that it can be used later when OsReauthCall() is
  // called. Note: This is safe because the |web_contents| is used before
  // exiting this method.
  // TODO(crbug.com/495290): Pass the native window directly to the
  // reauth-handling code.
  web_contents_ = web_contents;
  password_access_authenticator_.EnsureUserIsAuthenticated(
      GetReauthPurpose(api::passwords_private::PLAINTEXT_REASON_VIEW),
      base::BindOnce(
          &PasswordsPrivateDelegateImpl::OnRequestCredentialDetailsAuthResult,
          weak_ptr_factory_.GetWeakPtr(), ids, std::move(callback)));
}

void PasswordsPrivateDelegateImpl::OsReauthCall(
    password_manager::ReauthPurpose purpose,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback
        callback) {
#if BUILDFLAG(IS_WIN)
  AuthenticateUser(password_manager_util_win::GetMessageForLoginPrompt(purpose),
                   std::move(callback));
#elif BUILDFLAG(IS_MAC)
  AuthenticateUser(password_manager_util_mac::GetMessageForLoginPrompt(purpose),
                   std::move(callback));
#elif BUILDFLAG(IS_CHROMEOS)
  AuthenticateUser(std::u16string(), std::move(callback));
#else
  std::move(callback).Run(true);
#endif
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
      results.status = api::passwords_private::FamilyFetchStatus::
          FAMILY_FETCH_STATUS_UNKNOWN_ERROR;
      break;
    case FetchFamilyMembersRequestStatus::kSuccess:
      results.status = api::passwords_private::FamilyFetchStatus::
          FAMILY_FETCH_STATUS_SUCCESS;
      break;
    case FetchFamilyMembersRequestStatus::kNoFamily:
      results.status = api::passwords_private::FamilyFetchStatus::
          FAMILY_FETCH_STATUS_NO_MEMBERS;
  }
  if (request_status == FetchFamilyMembersRequestStatus::kSuccess) {
    for (const password_manager::RecipientInfo& family_member :
         family_members) {
      api::passwords_private::RecipientInfo recipient_info;
      recipient_info.user_id = family_member.user_id;
      recipient_info.email = family_member.email;
      recipient_info.display_name = family_member.user_name;
      recipient_info.profile_image_url = family_member.profile_image_url;

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
    DCHECK(get_saved_passwords_list_callbacks_.empty());
    DCHECK(get_password_exception_list_callbacks_.empty());
  }

  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnSavedPasswordsListChanged(current_entries_);
    router->OnPasswordExceptionsListChanged(current_exceptions_);
  }

  current_entries_initialized_ = true;
  InitializeIfNecessary();

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

  if (!client->GetPasswordFeatureManager()->IsOptedInForAccountStorage() ||
      SyncServiceFactory::GetForProfile(profile_)->IsSyncFeatureEnabled()) {
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

  // Desktop settings only offer bulk move, not invidual moves.
  saved_passwords_presenter_.MoveCredentialsToAccount(
      credentials_to_move,
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kExplicitlyTriggeredForMultiplePasswordsInSettings);
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
  // TODO(crbug/1445526): Get corresponding password forms for the id.
  // TODO(crbug/1445526): Call PasswordSenderService.
  return;
}

void PasswordsPrivateDelegateImpl::ImportPasswords(
    api::passwords_private::PasswordStoreSet to_store,
    ImportResultsCallback results_callback,
    content::WebContents* web_contents) {
  DCHECK_NE(api::passwords_private::PasswordStoreSet::
                PASSWORD_STORE_SET_DEVICE_AND_ACCOUNT,
            to_store);
  password_manager::PasswordForm::Store store_to_use =
      *ConvertToPasswordFormStores(to_store).begin();
  password_manager_porter_->Import(
      web_contents, store_to_use,
      base::BindOnce(&ConvertImportResults).Then(std::move(results_callback)));

  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  // Update the default store to the last used one.
  if (client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    client->GetPasswordFeatureManager()->SetDefaultPasswordStore(store_to_use);
  }
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
  // Save |web_contents| so that it can be used later when OsReauthCall() is
  // called. Note: This is safe because the |web_contents| is used before
  // exiting this method.
  // TODO(crbug.com/495290): Pass the native window directly to the
  // reauth-handling code.
  web_contents_ = web_contents;

  password_access_authenticator_.ForceUserReauthentication(
      password_manager::ReauthPurpose::IMPORT,
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
  // Save |web_contents| so that it can be used later when OsReauthCall() is
  // called. Note: This is safe because the |web_contents| is used before
  // exiting this method.
  // TODO(crbug.com/495290): Pass the native window directly to the
  // reauth-handling code.
  web_contents_ = web_contents;
  password_access_authenticator_.ForceUserReauthentication(
      password_manager::ReauthPurpose::EXPORT,
      base::BindOnce(&PasswordsPrivateDelegateImpl::OnExportPasswordsAuthResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(accepted_callback), web_contents));
}

api::passwords_private::ExportProgressStatus
PasswordsPrivateDelegateImpl::GetExportProgressStatus() {
  return ConvertStatus(password_manager_porter_->GetExportProgressStatus());
}

bool PasswordsPrivateDelegateImpl::IsOptedInForAccountStorage() {
  return password_manager::features_util::IsOptedInForAccountStorage(
      profile_->GetPrefs(), SyncServiceFactory::GetForProfile(profile_));
}

void PasswordsPrivateDelegateImpl::SetAccountStorageOptIn(
    bool opt_in,
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  if (opt_in ==
      client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    return;
  }
  if (!opt_in) {
    client->GetPasswordFeatureManager()
        ->OptOutOfAccountStorageAndClearSettings();
    return;
  }
  // The opt in pref is automatically set upon successful reauth.
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
  password_check_delegate_.StartPasswordCheck(std::move(callback));
}

api::passwords_private::PasswordCheckStatus
PasswordsPrivateDelegateImpl::GetPasswordCheckStatus() {
  return password_check_delegate_.GetPasswordCheckStatus();
}

void PasswordsPrivateDelegateImpl::SwitchBiometricAuthBeforeFillingState(
    content::WebContents* web_contents) {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  NOTIMPLEMENTED();
#else
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kBiometricAuthenticationForFilling));
  password_manager::PasswordAccessAuthenticator::AuthResultCallback callback =
      base::BindOnce(&ChangeBiometricAuthenticationBeforeFillingSetting,
                     profile_->GetPrefs());
  web_contents_ = web_contents;
  AuthenticateUser(GetMessageForBiometricAuthenticationBeforeFillingSetting(
                       profile_->GetPrefs()),
                   std::move(callback));
#endif
}

void PasswordsPrivateDelegateImpl::ShowAddShortcutDialog(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
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
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
#if !BUILDFLAG(IS_WIN)
  base::FilePath path(file_path);
#else
  base::FilePath path(base::UTF8ToWide(file_path));
#endif
  platform_util::ShowItemInFolder(browser->profile(), path);
}

password_manager::InsecureCredentialsManager*
PasswordsPrivateDelegateImpl::GetInsecureCredentialsManager() {
  return password_check_delegate_.GetInsecureCredentialsManager();
}

void PasswordsPrivateDelegateImpl::ExtendAuthValidity() {
  password_access_authenticator_.ExtendAuthValidity();
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
    std::move(callback).Run(absl::nullopt);
    return;
  }

  const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
  if (!entry) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (reason == api::passwords_private::PLAINTEXT_REASON_COPY) {
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
    bool authenticated) {
  if (!authenticated) {
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
        last_entry, api::passwords_private::PLAINTEXT_REASON_VIEW);
  }
  std::move(callback).Run(std::move(passwords));
}

void PasswordsPrivateDelegateImpl::OnExportPasswordsAuthResult(
    base::OnceCallback<void(const std::string&)> accepted_callback,
    content::WebContents* web_contents,
    bool authenticated) {
  if (!authenticated) {
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
    // TODO(crbug/1417650): Use specific enum for reauth_failed.
    // TODO(crbug/1417650): Record metric for reauth failed.
    result.status = password_manager::ImportResults::DISMISSED;
    std::move(results_callback).Run(ConvertImportResults(result));
    return;
  }

  CHECK(password_manager_porter_);
  password_manager_porter_->ContinueImport(
      selected_ids,
      base::BindOnce(&ConvertImportResults).Then(std::move(results_callback)));
}

void PasswordsPrivateDelegateImpl::OnAccountStorageOptInStateChanged() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnAccountStorageOptInStateChanged(IsOptedInForAccountStorage());
  }
}

void PasswordsPrivateDelegateImpl::OnReauthCompleted() {
  device_authenticator_.reset();
}

void PasswordsPrivateDelegateImpl::ExecuteFunction(base::OnceClosure callback) {
  if (is_initialized_) {
    std::move(callback).Run();
    return;
  }

  pre_initialization_callbacks_.emplace_back(std::move(callback));
}

void PasswordsPrivateDelegateImpl::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  SetCredentials(saved_passwords_presenter_.GetSavedCredentials());
}

void PasswordsPrivateDelegateImpl::OnWebAppInstalledWithOsHooks(
    const web_app::AppId& app_id) {
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

void PasswordsPrivateDelegateImpl::InitializeIfNecessary() {
  if (is_initialized_ || !current_entries_initialized_) {
    return;
  }

  is_initialized_ = true;

  for (base::OnceClosure& callback : pre_initialization_callbacks_) {
    std::move(callback).Run();
  }
  pre_initialization_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::EmitHistogramsForCredentialAccess(
    const CredentialUIEntry& entry,
    api::passwords_private::PlaintextReason reason) {
  syncer::SyncService* sync_service = nullptr;
  if (SyncServiceFactory::HasSyncService(profile_)) {
    sync_service = SyncServiceFactory::GetForProfile(profile_);
  }
  if (password_manager::sync_util::IsSyncAccountCredential(
          entry.GetURL(), entry.username, sync_service,
          IdentityManagerFactory::GetForProfile(profile_))) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialShown"));
  }

  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccessPasswordInSettings",
      ConvertPlaintextReason(reason),
      password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
}

void PasswordsPrivateDelegateImpl::AuthenticateUser(
    const std::u16string& message,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback
        callback) {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
  NOTIMPLEMENTED();
#else
  // Cancel any ongoing authentication attempt.
  if (device_authenticator_) {
    // TODO(crbug.com/1371026): Remove Cancel and instead simply destroy
    // |device_authenticator_|.
    device_authenticator_->Cancel(
        device_reauth::DeviceAuthRequester::kPasswordsInSettings);
  }
  device_authenticator_ = GetDeviceAuthenticator(web_contents_);

  base::OnceClosure on_reauth_completed =
      base::BindOnce(&PasswordsPrivateDelegateImpl::OnReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr());

  device_authenticator_->AuthenticateWithMessage(
      message, std::move(callback).Then(std::move(on_reauth_completed)));
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
                            domain_info.name = domain.name;
                            domain_info.url = domain.url.spec();
                            domain_info.signon_realm = domain.signon_realm;
                            return domain_info;
                          });
  entry.is_passkey = !credential.passkey_credential_id.empty();
  entry.username = base::UTF16ToUTF8(credential.username);
  if (entry.is_passkey) {
    entry.display_name = base::UTF16ToUTF8(credential.user_display_name);
  }
  entry.stored_in = extensions::StoreSetFromCredential(credential);
  if (!credential.federation_origin.opaque()) {
    std::u16string formatted_origin =
        url_formatter::FormatOriginForSecurityDisplay(
            credential.federation_origin,
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

    entry.federation_text = base::UTF16ToUTF8(formatted_origin);
  }
  entry.id = credential_id_generator_.GenerateId(std::move(credential));
  return entry;
}

}  // namespace extensions
