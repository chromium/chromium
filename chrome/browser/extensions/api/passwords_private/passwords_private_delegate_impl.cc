// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/plaintext_reason.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

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

password_manager::PlaintextReason ConvertPlaintextReason(
    extensions::api::passwords_private::PlaintextReason reason) {
  switch (reason) {
    case extensions::api::passwords_private::PLAINTEXT_REASON_VIEW:
      return password_manager::PlaintextReason::kView;
    case extensions::api::passwords_private::PLAINTEXT_REASON_COPY:
      return password_manager::PlaintextReason::kCopy;
    case extensions::api::passwords_private::PLAINTEXT_REASON_EDIT:
      return password_manager::PlaintextReason::kEdit;
    case extensions::api::passwords_private::PLAINTEXT_REASON_NONE:
      break;
  }

  NOTREACHED();
  return password_manager::PlaintextReason::kView;
}

// Gets all the existing keys in |generator| corresponding to |ids|. If no key
// is found for an id, it is simply ignored.
std::vector<std::string> GetSortKeys(
    const extensions::IdGenerator<std::string>& generator,
    const std::vector<int> ids) {
  std::vector<std::string> sort_keys;
  sort_keys.reserve(ids.size());
  for (int id : ids) {
    if (const std::string* sort_key = generator.TryGetKey(id))
      sort_keys.emplace_back(*sort_key);
  }
  return sort_keys;
}

}  // namespace

namespace extensions {

PasswordsPrivateDelegateImpl::PasswordsPrivateDelegateImpl(Profile* profile)
    : profile_(profile),
      password_manager_presenter_(
          std::make_unique<PasswordManagerPresenter>(this)),
      saved_passwords_presenter_(PasswordStoreFactory::GetForProfile(
                                     profile,
                                     ServiceAccessType::EXPLICIT_ACCESS),
                                 AccountPasswordStoreFactory::GetForProfile(
                                     profile,
                                     ServiceAccessType::EXPLICIT_ACCESS)),
      password_manager_porter_(std::make_unique<PasswordManagerPorter>(
          password_manager_presenter_.get(),
          base::BindRepeating(
              &PasswordsPrivateDelegateImpl::OnPasswordsExportProgress,
              base::Unretained(this)))),
      password_access_authenticator_(
          base::BindRepeating(&PasswordsPrivateDelegateImpl::OsReauthCall,
                              base::Unretained(this))),
      password_account_storage_settings_watcher_(
          std::make_unique<
              password_manager::PasswordAccountStorageSettingsWatcher>(
              profile_->GetPrefs(),
              SyncServiceFactory::GetForProfile(profile_),
              base::BindRepeating(&PasswordsPrivateDelegateImpl::
                                      OnAccountStorageOptInStateChanged,
                                  base::Unretained(this)))),
      password_check_delegate_(profile, &saved_passwords_presenter_),
      current_entries_initialized_(false),
      current_exceptions_initialized_(false),
      is_initialized_(false),
      web_contents_(nullptr) {
  password_manager_presenter_->Initialize();
  password_manager_presenter_->UpdatePasswordLists();
  saved_passwords_presenter_.Init();
}

PasswordsPrivateDelegateImpl::~PasswordsPrivateDelegateImpl() {}

void PasswordsPrivateDelegateImpl::SendSavedPasswordsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnSavedPasswordsListChanged(current_entries_);
}

void PasswordsPrivateDelegateImpl::GetSavedPasswordsList(
    UiEntriesCallback callback) {
  if (current_entries_initialized_)
    std::move(callback).Run(current_entries_);
  else
    get_saved_passwords_list_callbacks_.push_back(std::move(callback));
}

void PasswordsPrivateDelegateImpl::SendPasswordExceptionsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnPasswordExceptionsListChanged(current_exceptions_);
}

void PasswordsPrivateDelegateImpl::GetPasswordExceptionsList(
    ExceptionEntriesCallback callback) {
  if (current_exceptions_initialized_)
    std::move(callback).Run(current_exceptions_);
  else
    get_password_exception_list_callbacks_.push_back(std::move(callback));
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
    bool use_account_store,
    content::WebContents* web_contents) {
  password_manager::PasswordForm form;
  form.url = password_manager_util::StripAuthAndParams(
      password_manager_util::ConstructGURLWithScheme(url));
  form.signon_realm = password_manager::GetSignonRealm(form.url);
  form.username_value = username;
  form.password_value = password;
  form.in_store = use_account_store
                      ? password_manager::PasswordForm::Store::kAccountStore
                      : password_manager::PasswordForm::Store::kProfileStore;
  form.type = password_manager::PasswordForm::Type::kManuallyAdded;
  bool success = saved_passwords_presenter_.AddPassword(form);

  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  // Update the default store to the last used one.
  if (success &&
      client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    client->GetPasswordFeatureManager()->SetDefaultPasswordStore(form.in_store);
  }
  return success;
}

bool PasswordsPrivateDelegateImpl::ChangeSavedPassword(
    const std::vector<int>& ids,
    const api::passwords_private::ChangeSavedPasswordParams& params) {
  const std::vector<std::string> sort_keys =
      GetSortKeys(password_id_generator_, ids);

  DCHECK(!sort_keys.empty());
  if (ids.empty() || sort_keys.size() != ids.size())
    return false;

  std::vector<password_manager::PasswordForm> forms_to_change;

  for (const auto& key : sort_keys) {
    auto forms_for_key = password_manager_presenter_->GetPasswordsForKey(key);
    if (forms_for_key.empty())
      return false;
    for (const auto& form : forms_for_key)
      forms_to_change.push_back(*form);
  }

  std::u16string username = base::UTF8ToUTF16(params.username);
  std::u16string password = base::UTF8ToUTF16(params.password);
  if (params.note) {
    return saved_passwords_presenter_.EditSavedPasswords(
        forms_to_change, username, password, base::UTF8ToUTF16(*params.note));
  }
  return saved_passwords_presenter_.EditSavedPasswords(forms_to_change,
                                                       username, password);
}

void PasswordsPrivateDelegateImpl::RemoveSavedPasswords(
    const std::vector<int>& ids) {
  ExecuteFunction(base::BindOnce(
      &PasswordsPrivateDelegateImpl::RemoveSavedPasswordsInternal,
      base::Unretained(this), ids));
}

void PasswordsPrivateDelegateImpl::RemoveSavedPasswordsInternal(
    const std::vector<int>& ids) {
  password_manager_presenter_->RemoveSavedPasswords(
      GetSortKeys(password_id_generator_, ids));
}

void PasswordsPrivateDelegateImpl::RemovePasswordExceptions(
    const std::vector<int>& ids) {
  ExecuteFunction(base::BindOnce(
      &PasswordsPrivateDelegateImpl::RemovePasswordExceptionsInternal,
      base::Unretained(this), ids));
}

void PasswordsPrivateDelegateImpl::RemovePasswordExceptionsInternal(
    const std::vector<int>& ids) {
  password_manager_presenter_->RemovePasswordExceptions(
      GetSortKeys(exception_id_generator_, ids));
}

void PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrException() {
  ExecuteFunction(base::BindOnce(
      &PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrExceptionInternal,
      base::Unretained(this)));
}

void PasswordsPrivateDelegateImpl::
    UndoRemoveSavedPasswordOrExceptionInternal() {
  password_manager_presenter_->UndoRemoveSavedPasswordOrException();
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

void PasswordsPrivateDelegateImpl::OsReauthCall(
    password_manager::ReauthPurpose purpose,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback
        callback) {
#if BUILDFLAG(IS_WIN)
  DCHECK(web_contents_);
  bool result = password_manager_util_win::AuthenticateUser(
      web_contents_->GetTopLevelNativeWindow(), purpose);
  std::move(callback).Run(result);
#elif BUILDFLAG(IS_MAC)
  bool result = password_manager_util_mac::AuthenticateUser(purpose);
  std::move(callback).Run(result);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  bool result =
      IsOsReauthAllowedAsh(profile_, GetAuthTokenLifetimeForPurpose(purpose));
  std::move(callback).Run(result);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  IsOsReauthAllowedLacrosAsync(purpose, std::move(callback));
#else
  std::move(callback).Run(true);
#endif
}

Profile* PasswordsPrivateDelegateImpl::GetProfile() {
  return profile_;
}

void PasswordsPrivateDelegateImpl::SetPasswordList(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
        password_list) {
  // Create a list of PasswordUiEntry objects to send to observers.
  current_entries_.clear();

  for (const auto& form : password_list) {
    api::passwords_private::PasswordUiEntry entry;
    entry.urls = CreateUrlCollectionFromForm(*form);
    entry.username = base::UTF16ToUTF8(form->username_value);
    entry.password_note = base::UTF16ToUTF8(form->note.value);
    entry.id = password_id_generator_.GenerateId(
        password_manager::CreateSortKey(*form));
    entry.frontend_id = password_frontend_id_generator_.GenerateId(
        password_manager::CreateSortKey(*form,
                                        password_manager::IgnoreStore(true)));

    if (!form->federation_origin.opaque()) {
      entry.federation_text =
          std::make_unique<std::string>(l10n_util::GetStringFUTF8(
              IDS_PASSWORDS_VIA_FEDERATION, GetDisplayFederation(*form)));
    }

    entry.from_account_store = form->IsUsingAccountStore();

    current_entries_.push_back(std::move(entry));
  }

  SendSavedPasswordsList();

  DCHECK(!current_entries_initialized_ ||
         get_saved_passwords_list_callbacks_.empty());

  current_entries_initialized_ = true;
  InitializeIfNecessary();

  for (auto& callback : get_saved_passwords_list_callbacks_)
    std::move(callback).Run(current_entries_);
  get_saved_passwords_list_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
        password_exception_list) {
  // Creates a list of exceptions to send to observers.
  current_exceptions_.clear();

  for (const auto& form : password_exception_list) {
    api::passwords_private::ExceptionEntry current_exception_entry;
    current_exception_entry.urls = CreateUrlCollectionFromForm(*form);
    current_exception_entry.id = exception_id_generator_.GenerateId(
        password_manager::CreateSortKey(*form));
    current_exception_entry.frontend_id =
        exception_frontend_id_generator_.GenerateId(
            password_manager::CreateSortKey(
                *form, password_manager::IgnoreStore(true)));

    current_exception_entry.from_account_store = form->IsUsingAccountStore();
    current_exceptions_.push_back(std::move(current_exception_entry));
  }

  SendPasswordExceptionsList();

  DCHECK(!current_entries_initialized_ ||
         get_saved_passwords_list_callbacks_.empty());

  current_exceptions_initialized_ = true;
  InitializeIfNecessary();

  for (auto& callback : get_password_exception_list_callbacks_)
    std::move(callback).Run(current_exceptions_);
  get_password_exception_list_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::MovePasswordsToAccount(
    const std::vector<int>& ids,
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  std::vector<std::string> sort_keys;
  for (int id : ids) {
    if (const std::string* sort_key = password_id_generator_.TryGetKey(id))
      sort_keys.push_back(*sort_key);
  }
  password_manager_presenter_->MovePasswordsToAccountStore(sort_keys, client);
}

void PasswordsPrivateDelegateImpl::ImportPasswords(
    content::WebContents* web_contents) {
  password_manager_porter_->set_web_contents(web_contents);
  password_manager_porter_->Load();
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

void PasswordsPrivateDelegateImpl::CancelExportPasswords() {
  password_manager_porter_->CancelStore();
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

std::vector<api::passwords_private::InsecureCredential>
PasswordsPrivateDelegateImpl::GetCompromisedCredentials() {
  return password_check_delegate_.GetCompromisedCredentials();
}

std::vector<api::passwords_private::InsecureCredential>
PasswordsPrivateDelegateImpl::GetWeakCredentials() {
  return password_check_delegate_.GetWeakCredentials();
}

void PasswordsPrivateDelegateImpl::GetPlaintextInsecurePassword(
    api::passwords_private::InsecureCredential credential,
    api::passwords_private::PlaintextReason reason,
    content::WebContents* web_contents,
    PlaintextInsecurePasswordCallback callback) {
  // TODO(crbug.com/495290): Pass the native window directly to the
  // reauth-handling code.
  web_contents_ = web_contents;
  password_access_authenticator_.EnsureUserIsAuthenticated(
      GetReauthPurpose(reason),
      base::BindOnce(&PasswordsPrivateDelegateImpl::
                         OnGetPlaintextInsecurePasswordAuthResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(credential),
                     reason, std::move(callback)));
}

bool PasswordsPrivateDelegateImpl::ChangeInsecureCredential(
    const api::passwords_private::InsecureCredential& credential,
    base::StringPiece new_password) {
  return password_check_delegate_.ChangeInsecureCredential(credential,
                                                           new_password);
}

bool PasswordsPrivateDelegateImpl::RemoveInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) {
  return password_check_delegate_.RemoveInsecureCredential(credential);
}

bool PasswordsPrivateDelegateImpl::MuteInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) {
  return password_check_delegate_.MuteInsecureCredential(credential);
}

bool PasswordsPrivateDelegateImpl::UnmuteInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) {
  return password_check_delegate_.UnmuteInsecureCredential(credential);
}

void PasswordsPrivateDelegateImpl::RecordChangePasswordFlowStarted(
    const api::passwords_private::InsecureCredential& credential,
    bool is_manual_flow) {
  password_check_delegate_.RecordChangePasswordFlowStarted(credential,
                                                           is_manual_flow);
}

void PasswordsPrivateDelegateImpl::StartPasswordCheck(
    StartPasswordCheckCallback callback) {
  password_check_delegate_.StartPasswordCheck(std::move(callback));
}

void PasswordsPrivateDelegateImpl::StopPasswordCheck() {
  password_check_delegate_.StopPasswordCheck();
}

api::passwords_private::PasswordCheckStatus
PasswordsPrivateDelegateImpl::GetPasswordCheckStatus() {
  return password_check_delegate_.GetPasswordCheckStatus();
}

password_manager::InsecureCredentialsManager*
PasswordsPrivateDelegateImpl::GetInsecureCredentialsManager() {
  return password_check_delegate_.GetInsecureCredentialsManager();
}

void PasswordsPrivateDelegateImpl::OnPasswordsExportProgress(
    password_manager::ExportProgressStatus status,
    const std::string& folder_name) {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnPasswordsExportProgress(ConvertStatus(status), folder_name);
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

  // Request the password. When it is retrieved, ShowPassword() will be called.
  const std::string* sort_key = password_id_generator_.TryGetKey(id);
  if (!sort_key) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (reason == api::passwords_private::PLAINTEXT_REASON_COPY) {
    // In case of copy we don't need to give password back to UI. callback
    // will receive either empty string in case of success or null otherwise.
    // Copying occurs here so javascript doesn't need plaintext password.
    callback = base::BindOnce(
        [](PlaintextPasswordCallback callback,
           absl::optional<std::u16string> password) {
          if (!password) {
            std::move(callback).Run(absl::nullopt);
            return;
          }
          ui::ScopedClipboardWriter clipboard_writer(
              ui::ClipboardBuffer::kCopyPaste);
          clipboard_writer.WriteText(*password);
          clipboard_writer.MarkAsConfidential();
          std::move(callback).Run(std::u16string());
        },
        std::move(callback));
  }

  password_manager_presenter_->RequestPlaintextPassword(
      *sort_key, ConvertPlaintextReason(reason), std::move(callback));
}

void PasswordsPrivateDelegateImpl::OnExportPasswordsAuthResult(
    base::OnceCallback<void(const std::string&)> accepted_callback,
    content::WebContents* web_contents,
    bool authenticated) {
  if (!authenticated) {
    std::move(accepted_callback).Run(kReauthenticationFailed);
    return;
  }

  password_manager_porter_->set_web_contents(web_contents);
  bool accepted = password_manager_porter_->Store();
  std::move(accepted_callback)
      .Run(accepted ? std::string() : kExportInProgress);
}

void PasswordsPrivateDelegateImpl::OnGetPlaintextInsecurePasswordAuthResult(
    api::passwords_private::InsecureCredential credential,
    api::passwords_private::PlaintextReason reason,
    PlaintextInsecurePasswordCallback callback,
    bool authenticated) {
  if (!authenticated) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(password_check_delegate_.GetPlaintextInsecurePassword(
      std::move(credential)));
}

void PasswordsPrivateDelegateImpl::OnAccountStorageOptInStateChanged() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnAccountStorageOptInStateChanged(IsOptedInForAccountStorage());
  }
}

void PasswordsPrivateDelegateImpl::Shutdown() {
  password_account_storage_settings_watcher_.reset();
  password_manager_porter_.reset();
  password_manager_presenter_.reset();
}

IdGenerator<std::string>&
PasswordsPrivateDelegateImpl::GetPasswordIdGeneratorForTesting() {
  return password_id_generator_;
}

void PasswordsPrivateDelegateImpl::ExecuteFunction(base::OnceClosure callback) {
  if (is_initialized_) {
    std::move(callback).Run();
    return;
  }

  pre_initialization_callbacks_.emplace_back(std::move(callback));
}

void PasswordsPrivateDelegateImpl::InitializeIfNecessary() {
  if (is_initialized_ || !current_entries_initialized_ ||
      !current_exceptions_initialized_)
    return;

  is_initialized_ = true;

  for (base::OnceClosure& callback : pre_initialization_callbacks_)
    std::move(callback).Run();
  pre_initialization_callbacks_.clear();
}

}  // namespace extensions
