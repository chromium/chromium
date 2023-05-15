// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows.h must be first otherwise Win8 SDK breaks.
#include <windows.h>
#include <LM.h>
#include <ntsecapi.h>
#include <stddef.h>
#include <stdint.h>
#include <wincred.h>

// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32

#include "base/strings/utf_string_conversions.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/time/time.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/password_manager_util_win.h"
#include "chrome/grit/chromium_strings.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager_util_win {
namespace {

const unsigned kMaxPasswordRetries = 3;

struct PasswordCheckPrefs {
  PasswordCheckPrefs() : pref_last_changed_(0), blank_password_(false) {}

  void Read(PrefService* local_state);
  void Write(PrefService* local_state);

  int64_t pref_last_changed_;
  bool blank_password_;
};

// Validates whether a credential buffer contains the credentials for the
// currently signed in user.
class CredentialBufferValidator {
 public:
  CredentialBufferValidator();

  CredentialBufferValidator(const CredentialBufferValidator&) = delete;
  CredentialBufferValidator& operator=(const CredentialBufferValidator&) =
      delete;

  ~CredentialBufferValidator();

  // Returns ERROR_SUCCESS if the credential buffer given matches the
  // credentials of the user running Chrome.  Otherwise an error describing
  // the issue.
  DWORD IsValid(ULONG auth_package, void* cred_buffer, ULONG cred_length);

 private:
  std::unique_ptr<char[]> GetTokenInformation(HANDLE token);

  // Name of app calling LsaLogonUser().  In this case, "chrome".
  LSA_STRING name_;

  // Handle to LSA server.
  HANDLE lsa_ = INVALID_HANDLE_VALUE;

  // Buffer holding information about the current process token.
  std::unique_ptr<char[]> cur_token_info_;
};

CredentialBufferValidator::CredentialBufferValidator() {
  // Windows 7 does not support pseudo tokens with GetTokenInformation(), so
  // make sure to open a real token.
  HANDLE token;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    DLOG(ERROR) << "Unable to obtain process token " << GetLastError();
    return;
  }

  cur_token_info_ = GetTokenInformation(token);
  CloseHandle(token);
  if (!cur_token_info_) {
    DLOG(ERROR) << "Unable to obtain current token info " << GetLastError();
    return;
  }

  NTSTATUS sts = LsaConnectUntrusted(&lsa_);
  if (sts != ERROR_SUCCESS) {
    lsa_ = INVALID_HANDLE_VALUE;
    return;
  }

  name_.Buffer = const_cast<PCHAR>("Chrome");
  name_.Length = strlen(name_.Buffer);
  name_.MaximumLength = name_.Length + 1;
}

CredentialBufferValidator::~CredentialBufferValidator() {
  if (lsa_ != INVALID_HANDLE_VALUE)
    LsaDeregisterLogonProcess(lsa_);
}

DWORD CredentialBufferValidator::IsValid(ULONG auth_package,
                                         void* auth_buffer,
                                         ULONG auth_length) {
  if (lsa_ == INVALID_HANDLE_VALUE)
    return ERROR_LOGON_FAILURE;

  NTSTATUS sts;
  NTSTATUS substs;
  TOKEN_SOURCE source;
  void* profile_buffer = nullptr;
  ULONG profile_buffer_length = 0;
  QUOTA_LIMITS limits;
  LUID luid;
  HANDLE token = INVALID_HANDLE_VALUE;

  strcpy_s(source.SourceName, std::size(source.SourceName), "Chrome");
  if (!AllocateLocallyUniqueId(&source.SourceIdentifier))
    return GetLastError();

  sts = LsaLogonUser(lsa_, &name_, Interactive, auth_package, auth_buffer,
                     auth_length, nullptr, &source, &profile_buffer,
                     &profile_buffer_length, &luid, &token, &limits, &substs);
  LsaFreeReturnBuffer(profile_buffer);
  std::unique_ptr<char[]> logon_token_info = GetTokenInformation(token);
  CloseHandle(token);
  if (sts != S_OK)
    return LsaNtStatusToWinError(sts);
  if (!logon_token_info)
    return ERROR_NOT_ENOUGH_MEMORY;

  PSID cur_sid = reinterpret_cast<TOKEN_USER*>(cur_token_info_.get())->User.Sid;
  PSID logon_sid =
      reinterpret_cast<TOKEN_USER*>(logon_token_info.get())->User.Sid;
  return EqualSid(cur_sid, logon_sid) ? ERROR_SUCCESS : ERROR_LOGON_FAILURE;
}

std::unique_ptr<char[]> CredentialBufferValidator::GetTokenInformation(
    HANDLE token) {
  DWORD token_info_length = 0;
  ::GetTokenInformation(token, TokenUser, nullptr, 0, &token_info_length);
  if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
    return nullptr;

  std::unique_ptr<char[]> token_info_buffer(new char[token_info_length]);
  if (!::GetTokenInformation(token, TokenUser, token_info_buffer.get(),
                             token_info_length, &token_info_length)) {
    return nullptr;
  }

  return token_info_buffer;
}

void PasswordCheckPrefs::Read(PrefService* local_state) {
  blank_password_ =
      local_state->GetBoolean(password_manager::prefs::kOsPasswordBlank);
  pref_last_changed_ =
      local_state->GetInt64(password_manager::prefs::kOsPasswordLastChanged);
}

void PasswordCheckPrefs::Write(PrefService* local_state) {
  local_state->SetBoolean(password_manager::prefs::kOsPasswordBlank,
                          blank_password_);
  local_state->SetInt64(password_manager::prefs::kOsPasswordLastChanged,
                        pref_last_changed_);
}

int64_t GetPasswordLastChanged(const WCHAR* username) {
  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  LPUSER_INFO_1 user_info = NULL;
  DWORD age = 0;

  NET_API_STATUS ret = NetUserGetInfo(NULL, username, 1,
                                      reinterpret_cast<LPBYTE*>(&user_info));

  if (ret == NERR_Success) {
    // Returns seconds since last password change.
    age = user_info->usri1_password_age;
    NetApiBufferFree(user_info);
  } else {
    return -1;
  }

  base::Time changed = base::Time::Now() - base::Seconds(age);

  return changed.ToInternalValue();
}

bool CheckBlankPasswordWithPrefs(const WCHAR* username,
                                 PasswordCheckPrefs* prefs) {
  // If the user name has a backslash, then it is of the form DOMAIN\username.
  // NetUserGetInfo() (called from GetPasswordLastChanged()) as well as
  // LogonUser() below only wants the username portion.
  LPCWSTR backslash = wcschr(username, L'\\');
  if (backslash)
    username = backslash + 1;

  int64_t last_changed = GetPasswordLastChanged(username);

  // If we cannot determine when the password was last changed
  // then assume the password is not blank
  if (last_changed == -1)
    return false;

  bool blank_password = prefs->blank_password_;
  bool need_recheck = true;
  if (prefs->pref_last_changed_ > 0 &&
      last_changed <= prefs->pref_last_changed_) {
    need_recheck = false;
  }

  if (need_recheck) {
    // Mitigate the issues caused by loading DLLs on a background thread
    // (http://crbug/973868).
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    HANDLE handle = INVALID_HANDLE_VALUE;

    // Attempt to login using blank password.
    DWORD logon_result = LogonUser(username,
                                   L".",
                                   L"",
                                   LOGON32_LOGON_INTERACTIVE,
                                   LOGON32_PROVIDER_DEFAULT,
                                   &handle);

    auto last_error = GetLastError();
    // Win XP and later return ERROR_ACCOUNT_RESTRICTION for blank password.
    if (logon_result)
      CloseHandle(handle);

    // In the case the password is blank, then LogonUser returns a failure,
    // handle is INVALID_HANDLE_VALUE, and GetLastError() is
    // ERROR_ACCOUNT_RESTRICTION.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms681385
    blank_password = (logon_result ||
                      last_error == ERROR_ACCOUNT_RESTRICTION);
  }

  // Account for clock skew between pulling the password age and
  // writing to the preferences by adding a small skew factor here.
  last_changed += base::Time::kMicrosecondsPerSecond;

  // Update the preferences with new values.
  prefs->pref_last_changed_ = last_changed;
  prefs->blank_password_ = blank_password;
  return blank_password;
}

// Wrapper around CheckBlankPasswordWithPrefs to be called on UI thread.
bool CheckBlankPassword(const WCHAR* username) {
  PrefService* local_state = g_browser_process->local_state();
  PasswordCheckPrefs prefs;
  prefs.Read(local_state);
  bool result = CheckBlankPasswordWithPrefs(username, &prefs);
  prefs.Write(local_state);
  return result;
}

}  // namespace

bool AuthenticateUser(gfx::NativeWindow window,
                      const std::u16string& password_prompt) {
  bool retval = false;
  WCHAR cur_username[CREDUI_MAX_USERNAME_LENGTH + 1] = {};
  DWORD cur_username_length = std::size(cur_username);

  // If this is a standlone workstation, it's possible the current user has no
  // password, so check here and allow it.
  if (!GetUserNameEx(NameSamCompatible, cur_username, &cur_username_length)) {
    DLOG(ERROR) << "Unable to obtain username " << GetLastError();
    return false;
  }

  if (!base::win::IsEnrolledToDomain() && CheckBlankPassword(cur_username))
    return true;

  // Build the strings to display in the credential UI.  If these strings are
  // left empty on domain joined machines, CredUIPromptForWindowsCredentials()
  // fails to run.
  std::u16string product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  CREDUI_INFO cui;
  cui.cbSize = sizeof(cui);
  cui.hwndParent = window->GetHost()->GetAcceleratedWidget();
  cui.pszMessageText = base::as_wcstr(password_prompt);
  cui.pszCaptionText = base::as_wcstr(product_name);
  cui.hbmBanner = nullptr;

  // Never consider the current scope as hung. The hang watching deadline (if
  // any) is not valid since the user can take unbounded time to answer the
  // password prompt (http://crbug.com/806174)
  base::HangWatcher::InvalidateActiveExpectations();

  CredentialBufferValidator validator;

  DWORD err = 0;
  size_t tries = 0;
  do {
    tries++;

    // Show credential prompt, displaying error from previous try if needed.
    // TODO(wfh): Make sure we support smart cards here.
    ULONG auth_package = 0;
    LPVOID cred_buffer = nullptr;
    ULONG cred_buffer_size = 0;
    err = CredUIPromptForWindowsCredentials(
        &cui, err, &auth_package, nullptr, 0, &cred_buffer, &cred_buffer_size,
        nullptr, CREDUIWIN_ENUMERATE_CURRENT_USER);
    if (err != ERROR_SUCCESS)
      break;

    // While CredUIPromptForWindowsCredentials() shows the currently logged
    // on user by default, it can be changed at runtime.  This is important,
    // as it allows users to change to a different type of authentication
    // mechanism, such as PIN or smartcard.  However, this also allows the
    // user to change to a completely different account on the machine.  Make
    // sure the user authenticated with the credentials of the currently
    // logged on user.
    err = validator.IsValid(auth_package, cred_buffer, cred_buffer_size);
    retval = err == ERROR_SUCCESS;
  } while (!retval && tries < kMaxPasswordRetries);

  return retval;
}

std::u16string GetMessageForLoginPrompt(
    password_manager::ReauthPurpose purpose) {
  switch (purpose) {
    case password_manager::ReauthPurpose::VIEW_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
    case password_manager::ReauthPurpose::COPY_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_COPY_AUTHENTICATION_PROMPT);
    case password_manager::ReauthPurpose::EDIT_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EDIT_AUTHENTICATION_PROMPT);
    case password_manager::ReauthPurpose::EXPORT:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EXPORT_AUTHENTICATION_PROMPT);
    case password_manager::ReauthPurpose::IMPORT:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_IMPORT_AUTHENTICATION_PROMPT);
  }
}

}  // namespace password_manager_util_win
