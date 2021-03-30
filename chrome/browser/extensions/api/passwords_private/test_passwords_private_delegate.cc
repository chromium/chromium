// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "ui/base/l10n/time_format.h"

namespace extensions {

namespace {

using ui::TimeFormat;

constexpr size_t kNumMocks = 3;

api::passwords_private::PasswordUiEntry CreateEntry(int id) {
  api::passwords_private::PasswordUiEntry entry;
  entry.urls.shown = "test" + base::NumberToString(id) + ".com";
  entry.urls.origin = "http://" + entry.urls.shown + "/login";
  entry.urls.link = entry.urls.origin;
  entry.username = "testName" + base::NumberToString(id);
  entry.id = id;
  entry.frontend_id = id;
  return entry;
}

api::passwords_private::ExceptionEntry CreateException(int id) {
  api::passwords_private::ExceptionEntry exception;
  exception.urls.shown = "exception" + base::NumberToString(id) + ".com";
  exception.urls.origin = "http://" + exception.urls.shown + "/login";
  exception.urls.link = exception.urls.origin;
  exception.id = id;
  exception.frontend_id = id;
  return exception;
}
}  // namespace

TestPasswordsPrivateDelegate::TestPasswordsPrivateDelegate()
    : profile_(nullptr) {
  // Create mock data.
  for (size_t i = 0; i < kNumMocks; i++) {
    current_entries_.push_back(CreateEntry(i));
    current_exceptions_.push_back(CreateException(i));
  }
}
TestPasswordsPrivateDelegate::~TestPasswordsPrivateDelegate() = default;

void TestPasswordsPrivateDelegate::GetSavedPasswordsList(
    UiEntriesCallback callback) {
  std::move(callback).Run(current_entries_);
}

void TestPasswordsPrivateDelegate::GetPasswordExceptionsList(
    ExceptionEntriesCallback callback) {
  std::move(callback).Run(current_exceptions_);
}

bool TestPasswordsPrivateDelegate::ChangeSavedPassword(
    const std::vector<int>& ids,
    const std::u16string& new_username,
    const std::u16string& new_password) {
  for (int id : ids) {
    if (static_cast<size_t>(id) >= current_entries_.size()) {
      return false;
    }
  }
  return !new_password.empty() && !ids.empty();
}

void TestPasswordsPrivateDelegate::RemoveSavedPasswords(
    const std::vector<int>& ids) {
  if (current_entries_.empty())
    return;

  // Since this is just mock data, remove the first |ids.size()| elements
  // regardless of the data contained.
  auto first_remaining = (ids.size() <= current_entries_.size())
                             ? current_entries_.begin() + ids.size()
                             : current_entries_.end();
  last_deleted_entries_batch_.assign(
      std::make_move_iterator(current_entries_.begin()),
      std::make_move_iterator(first_remaining));
  current_entries_.erase(current_entries_.begin(), first_remaining);
  SendSavedPasswordsList();
}

void TestPasswordsPrivateDelegate::RemovePasswordExceptions(
    const std::vector<int>& ids) {
  if (current_exceptions_.empty())
    return;

  // Since this is just mock data, remove the first |ids.size()| elements
  // regardless of the data contained.
  auto first_remaining = (ids.size() <= current_exceptions_.size())
                             ? current_exceptions_.begin() + ids.size()
                             : current_exceptions_.end();
  last_deleted_exceptions_batch_.assign(
      std::make_move_iterator(current_exceptions_.begin()),
      std::make_move_iterator(first_remaining));
  current_exceptions_.erase(current_exceptions_.begin(), first_remaining);
  SendPasswordExceptionsList();
}

// Simplified version of undo logic, only use for testing.
void TestPasswordsPrivateDelegate::UndoRemoveSavedPasswordOrException() {
  if (!last_deleted_entries_batch_.empty()) {
    current_entries_.insert(
        current_entries_.begin(),
        std::make_move_iterator(last_deleted_entries_batch_.begin()),
        std::make_move_iterator(last_deleted_entries_batch_.end()));
    last_deleted_entries_batch_.clear();
    SendSavedPasswordsList();
  } else if (!last_deleted_exceptions_batch_.empty()) {
    current_exceptions_.insert(
        current_exceptions_.begin(),
        std::make_move_iterator(last_deleted_exceptions_batch_.begin()),
        std::make_move_iterator(last_deleted_exceptions_batch_.end()));
    last_deleted_exceptions_batch_.clear();
    SendPasswordExceptionsList();
  }
}

void TestPasswordsPrivateDelegate::RequestPlaintextPassword(
    int id,
    api::passwords_private::PlaintextReason reason,
    PlaintextPasswordCallback callback,
    content::WebContents* web_contents) {
  // Return a mocked password value.
  std::move(callback).Run(plaintext_password_);
}

void TestPasswordsPrivateDelegate::MovePasswordsToAccount(
    const std::vector<int>& ids,
    content::WebContents* web_contents) {
  last_moved_passwords_ = ids;
}

void TestPasswordsPrivateDelegate::ImportPasswords(
    content::WebContents* web_contents) {
  // The testing of password importing itself should be handled via
  // |PasswordManagerPorter|.
  import_passwords_triggered_ = true;
}

void TestPasswordsPrivateDelegate::ExportPasswords(
    base::OnceCallback<void(const std::string&)> callback,
    content::WebContents* web_contents) {
  // The testing of password exporting itself should be handled via
  // |PasswordManagerPorter|.
  export_passwords_triggered_ = true;
  std::move(callback).Run(std::string());
}

void TestPasswordsPrivateDelegate::CancelExportPasswords() {
  cancel_export_passwords_triggered_ = true;
}

api::passwords_private::ExportProgressStatus
TestPasswordsPrivateDelegate::GetExportProgressStatus() {
  // The testing of password exporting itself should be handled via
  // |PasswordManagerPorter|.
  return api::passwords_private::ExportProgressStatus::
      EXPORT_PROGRESS_STATUS_IN_PROGRESS;
}

bool TestPasswordsPrivateDelegate::IsOptedInForAccountStorage() {
  return is_opted_in_for_account_storage_;
}

void TestPasswordsPrivateDelegate::SetAccountStorageOptIn(
    bool opt_in,
    content::WebContents* web_contents) {
  is_opted_in_for_account_storage_ = opt_in;
}

std::vector<api::passwords_private::InsecureCredential>
TestPasswordsPrivateDelegate::GetCompromisedCredentials() {
  api::passwords_private::InsecureCredential credential;
  credential.username = "alice";
  credential.formatted_origin = "example.com";
  credential.detailed_origin = "https://example.com";
  credential.is_android_credential = false;
  credential.change_password_url =
      std::make_unique<std::string>("https://example.com/change-password");
  credential.compromised_info =
      std::make_unique<api::passwords_private::CompromisedInfo>();
  // Mar 03 2020 12:00:00 UTC
  credential.compromised_info->compromise_time = 1583236800000;
  credential.compromised_info->elapsed_time_since_compromise =
      base::UTF16ToUTF8(TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                           TimeFormat::LENGTH_LONG,
                                           base::TimeDelta::FromDays(3)));
  credential.compromised_info->compromise_type =
      api::passwords_private::COMPROMISE_TYPE_LEAKED;
  std::vector<api::passwords_private::InsecureCredential> credentials;
  credentials.push_back(std::move(credential));
  return credentials;
}

std::vector<api::passwords_private::InsecureCredential>
TestPasswordsPrivateDelegate::GetWeakCredentials() {
  api::passwords_private::InsecureCredential credential;
  credential.username = "bob";
  credential.formatted_origin = "example.com";
  credential.detailed_origin = "https://example.com";
  credential.is_android_credential = false;
  credential.change_password_url =
      std::make_unique<std::string>("https://example.com/change-password");
  std::vector<api::passwords_private::InsecureCredential> credentials;
  credentials.push_back(std::move(credential));
  return credentials;
}

void TestPasswordsPrivateDelegate::GetPlaintextInsecurePassword(
    api::passwords_private::InsecureCredential credential,
    api::passwords_private::PlaintextReason reason,
    content::WebContents* web_contents,
    PlaintextInsecurePasswordCallback callback) {
  // Return a mocked password value.
  if (!plaintext_password_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  credential.password =
      std::make_unique<std::string>(base::UTF16ToUTF8(*plaintext_password_));
  std::move(callback).Run(std::move(credential));
}

// Fake implementation of ChangeInsecureCredential. This succeeds if the
// delegate knows of a insecure credential with the same id.
bool TestPasswordsPrivateDelegate::ChangeInsecureCredential(
    const api::passwords_private::InsecureCredential& credential,
    base::StringPiece new_password) {
  return std::any_of(insecure_credentials_.begin(), insecure_credentials_.end(),
                     [&credential](const auto& insecure_credential) {
                       return insecure_credential.id == credential.id;
                     });
}

// Fake implementation of RemoveInsecureCredential. This succeeds if the
// delegate knows of a insecure credential with the same id.
bool TestPasswordsPrivateDelegate::RemoveInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) {
  return base::EraseIf(insecure_credentials_,
                       [&credential](const auto& insecure_credential) {
                         return insecure_credential.id == credential.id;
                       }) != 0;
}

void TestPasswordsPrivateDelegate::StartPasswordCheck(
    StartPasswordCheckCallback callback) {
  start_password_check_triggered_ = true;
  std::move(callback).Run(start_password_check_state_);
}

void TestPasswordsPrivateDelegate::StopPasswordCheck() {
  stop_password_check_triggered_ = true;
}

api::passwords_private::PasswordCheckStatus
TestPasswordsPrivateDelegate::GetPasswordCheckStatus() {
  api::passwords_private::PasswordCheckStatus status;
  status.state = api::passwords_private::PASSWORD_CHECK_STATE_RUNNING;
  status.already_processed = std::make_unique<int>(5);
  status.remaining_in_queue = std::make_unique<int>(10);
  status.elapsed_time_since_last_check =
      std::make_unique<std::string>(base::UTF16ToUTF8(TimeFormat::Simple(
          TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT,
          base::TimeDelta::FromMinutes(5))));
  return status;
}

password_manager::InsecureCredentialsManager*
TestPasswordsPrivateDelegate::GetInsecureCredentialsManager() {
  return nullptr;
}

void TestPasswordsPrivateDelegate::SetProfile(Profile* profile) {
  profile_ = profile;
}

void TestPasswordsPrivateDelegate::SetOptedInForAccountStorage(bool opted_in) {
  is_opted_in_for_account_storage_ = opted_in;
}

void TestPasswordsPrivateDelegate::AddCompromisedCredential(int id) {
  api::passwords_private::InsecureCredential cred;
  cred.id = id;
  insecure_credentials_.push_back(std::move(cred));
}

void TestPasswordsPrivateDelegate::SendSavedPasswordsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnSavedPasswordsListChanged(current_entries_);
}

void TestPasswordsPrivateDelegate::SendPasswordExceptionsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnPasswordExceptionsListChanged(current_exceptions_);
}

}  // namespace extensions
