// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
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

}  // namespace

namespace extensions {

PasswordsPrivateDelegateImpl::PasswordsPrivateDelegateImpl(Profile* profile)
    : profile_(profile),
      password_manager_presenter_(
          std::make_unique<PasswordManagerPresenter>(this)),
      password_manager_porter_(std::make_unique<PasswordManagerPorter>(
          password_manager_presenter_.get(),
          base::BindRepeating(
              &PasswordsPrivateDelegateImpl::OnPasswordsExportProgress,
              base::Unretained(this)))),
      password_access_authenticator_(
          base::BindRepeating(&PasswordsPrivateDelegateImpl::OsReauthCall,
                              base::Unretained(this))),
      current_entries_initialized_(false),
      current_exceptions_initialized_(false),
      is_initialized_(false),
      web_contents_(nullptr) {
  password_manager_presenter_->Initialize();
  password_manager_presenter_->UpdatePasswordLists();
}

PasswordsPrivateDelegateImpl::~PasswordsPrivateDelegateImpl() {}

void PasswordsPrivateDelegateImpl::SendSavedPasswordsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnSavedPasswordsListChanged(current_entries_);
}

void PasswordsPrivateDelegateImpl::GetSavedPasswordsList(
    const UiEntriesCallback& callback) {
  if (current_entries_initialized_)
    callback.Run(current_entries_);
  else
    get_saved_passwords_list_callbacks_.push_back(callback);
}

void PasswordsPrivateDelegateImpl::SendPasswordExceptionsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnPasswordExceptionsListChanged(current_exceptions_);
}

void PasswordsPrivateDelegateImpl::GetPasswordExceptionsList(
    const ExceptionEntriesCallback& callback) {
  if (current_exceptions_initialized_)
    callback.Run(current_exceptions_);
  else
    get_password_exception_list_callbacks_.push_back(callback);
}

void PasswordsPrivateDelegateImpl::RemoveSavedPassword(int id) {
  ExecuteFunction(
      base::Bind(&PasswordsPrivateDelegateImpl::RemoveSavedPasswordInternal,
                 base::Unretained(this), id));
}

void PasswordsPrivateDelegateImpl::RemoveSavedPasswordInternal(int id) {
  const std::string* sort_key = password_id_generator_.TryGetSortKey(id);
  if (sort_key)
    password_manager_presenter_->RemoveSavedPassword(*sort_key);
}

void PasswordsPrivateDelegateImpl::RemovePasswordException(int id) {
  ExecuteFunction(
      base::Bind(&PasswordsPrivateDelegateImpl::RemovePasswordExceptionInternal,
                 base::Unretained(this), id));
}

void PasswordsPrivateDelegateImpl::RemovePasswordExceptionInternal(int id) {
  const std::string* sort_key = exception_id_generator_.TryGetSortKey(id);
  if (sort_key)
    password_manager_presenter_->RemovePasswordException(*sort_key);
}

void PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrException() {
  ExecuteFunction(base::Bind(
      &PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrExceptionInternal,
      base::Unretained(this)));
}

void PasswordsPrivateDelegateImpl::
    UndoRemoveSavedPasswordOrExceptionInternal() {
  password_manager_presenter_->UndoRemoveSavedPasswordOrException();
}

void PasswordsPrivateDelegateImpl::RequestShowPassword(
    int id,
    content::WebContents* web_contents) {
  ExecuteFunction(
      base::Bind(&PasswordsPrivateDelegateImpl::RequestShowPasswordInternal,
                 base::Unretained(this), id, web_contents));
}

void PasswordsPrivateDelegateImpl::RequestShowPasswordInternal(
    int id,
    content::WebContents* web_contents) {
  // Save |web_contents| so that the call to RequestShowPassword() below
  // can use this value by calling GetNativeWindow(). Note: This is safe because
  // GetNativeWindow() will only be called immediately from
  // RequestShowPassword().
  // TODO(stevenjb): Pass this directly to RequestShowPassword(); see
  // crbug.com/495290.
  web_contents_ = web_contents;
  if (!password_access_authenticator_.EnsureUserIsAuthenticated(
          password_manager::ReauthPurpose::VIEW_PASSWORD)) {
    return;
  }

  // Request the password. When it is retrieved, ShowPassword() will be called.
  const std::string* sort_key = password_id_generator_.TryGetSortKey(id);
  if (sort_key)
    password_manager_presenter_->RequestShowPassword(*sort_key);
}

bool PasswordsPrivateDelegateImpl::OsReauthCall(
    password_manager::ReauthPurpose purpose) {
#if defined(OS_WIN)
  return password_manager_util_win::AuthenticateUser(GetNativeWindow(),
                                                     purpose);
#elif defined(OS_MACOSX)
  return password_manager_util_mac::AuthenticateUser(purpose);
#else
  return true;
#endif
}

Profile* PasswordsPrivateDelegateImpl::GetProfile() {
  return profile_;
}

void PasswordsPrivateDelegateImpl::ShowPassword(
    const std::string& sort_key,
    const base::string16& password_value) {
  auto* router = PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnPlaintextPasswordFetched(
        password_id_generator_.GenerateId(sort_key),
        base::UTF16ToUTF8(password_value));
  }
}

void PasswordsPrivateDelegateImpl::SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  // Create a list of PasswordUiEntry objects to send to observers.
  current_entries_.clear();

  for (const auto& form : password_list) {
    api::passwords_private::PasswordUiEntry entry;
    entry.login_pair.urls = CreateUrlCollectionFromForm(*form);
    entry.login_pair.username = base::UTF16ToUTF8(form->username_value);
    entry.id = password_id_generator_.GenerateId(
        password_manager::CreateSortKey(*form));
    entry.num_characters_in_password = form->password_value.length();

    if (!form->federation_origin.opaque()) {
      entry.federation_text.reset(new std::string(l10n_util::GetStringFUTF8(
          IDS_PASSWORDS_VIA_FEDERATION,
          base::UTF8ToUTF16(form->federation_origin.host()))));
    }

    current_entries_.push_back(std::move(entry));
  }

  SendSavedPasswordsList();

  DCHECK(!current_entries_initialized_ ||
         get_saved_passwords_list_callbacks_.empty());

  current_entries_initialized_ = true;
  InitializeIfNecessary();

  for (const auto& callback : get_saved_passwords_list_callbacks_)
    callback.Run(current_entries_);
  get_saved_passwords_list_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
        password_exception_list) {
  // Creates a list of exceptions to send to observers.
  current_exceptions_.clear();

  for (const auto& form : password_exception_list) {
    api::passwords_private::ExceptionEntry current_exception_entry;
    current_exception_entry.urls = CreateUrlCollectionFromForm(*form);
    current_exception_entry.id = exception_id_generator_.GenerateId(
        password_manager::CreateSortKey(*form));
    current_exceptions_.push_back(std::move(current_exception_entry));
  }

  SendPasswordExceptionsList();

  DCHECK(!current_entries_initialized_ ||
         get_saved_passwords_list_callbacks_.empty());

  current_exceptions_initialized_ = true;
  InitializeIfNecessary();

  for (const auto& callback : get_password_exception_list_callbacks_)
    callback.Run(current_exceptions_);
  get_password_exception_list_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::ImportPasswords(
    content::WebContents* web_contents) {
  password_manager_porter_->set_web_contents(web_contents);
  password_manager_porter_->Load();
}

void PasswordsPrivateDelegateImpl::ExportPasswords(
    base::OnceCallback<void(const std::string&)> callback,
    content::WebContents* web_contents) {
  // Save |web_contents| so that it can be used later when GetNativeWindow() is
  // called. Note: This is safe because the |web_contents| is used before
  // exiting this method. TODO(crbug.com/495290): Pass the native window
  // directly to the reauth-handling code.
  web_contents_ = web_contents;
  if (!password_access_authenticator_.ForceUserReauthentication(
          password_manager::ReauthPurpose::EXPORT)) {
    std::move(callback).Run(kReauthenticationFailed);
    return;
  }

  password_manager_porter_->set_web_contents(web_contents);
  bool accepted = password_manager_porter_->Store();
  std::move(callback).Run(accepted ? std::string() : kExportInProgress);
}

void PasswordsPrivateDelegateImpl::CancelExportPasswords() {
  password_manager_porter_->CancelStore();
}

api::passwords_private::ExportProgressStatus
PasswordsPrivateDelegateImpl::GetExportProgressStatus() {
  return ConvertStatus(password_manager_porter_->GetExportProgressStatus());
}

#if !defined(OS_ANDROID)
gfx::NativeWindow PasswordsPrivateDelegateImpl::GetNativeWindow() const {
  DCHECK(web_contents_);
  return web_contents_->GetTopLevelNativeWindow();
}
#endif

void PasswordsPrivateDelegateImpl::OnPasswordsExportProgress(
    password_manager::ExportProgressStatus status,
    const std::string& folder_name) {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnPasswordsExportProgress(ConvertStatus(status), folder_name);
  }
}

void PasswordsPrivateDelegateImpl::Shutdown() {
  password_manager_presenter_.reset();
  password_manager_porter_.reset();
}

void PasswordsPrivateDelegateImpl::SetOsReauthCallForTesting(
    PasswordAccessAuthenticator::ReauthCallback os_reauth_call) {
  password_access_authenticator_.SetOsReauthCallForTesting(
      std::move(os_reauth_call));
}

void PasswordsPrivateDelegateImpl::ExecuteFunction(
    const base::Closure& callback) {
  if (is_initialized_) {
    callback.Run();
    return;
  }

  pre_initialization_callbacks_.push_back(callback);
}

void PasswordsPrivateDelegateImpl::InitializeIfNecessary() {
  if (is_initialized_ || !current_entries_initialized_ ||
      !current_exceptions_initialized_)
    return;

  is_initialized_ = true;

  for (const base::Closure& callback : pre_initialization_callbacks_)
    callback.Run();
  pre_initialization_callbacks_.clear();
}

}  // namespace extensions
