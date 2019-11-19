// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows.h must be first otherwise Win8 SDK breaks.
#include <windows.h>
#include <LM.h>
#include <ntsecapi.h>
#include <objbase.h>  // For CoTaskMemFree()
#include <stddef.h>
#include <stdint.h>
#include <wincred.h>

// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32

#include <memory>

#include "chrome/browser/password_manager/password_manager_util_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/time/time.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/chromium_strings.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager_util_win {
namespace {

enum OsPasswordStatus {
  PASSWORD_STATUS_UNKNOWN = 0,
  PASSWORD_STATUS_UNSUPPORTED,
  PASSWORD_STATUS_BLANK,
  PASSWORD_STATUS_NONBLANK,
  PASSWORD_STATUS_WIN_DOMAIN,
  // NOTE: Add new status types only immediately above this line. Also,
  // make sure the enum list in tools/histogram/histograms.xml is
  // updated with any change in here.
  MAX_PASSWORD_STATUS
};

const unsigned kMaxPasswordRetries = 3;

const unsigned kCredUiDefaultFlags =
    CREDUI_FLAGS_GENERIC_CREDENTIALS |
    CREDUI_FLAGS_EXCLUDE_CERTIFICATES |
    CREDUI_FLAGS_KEEP_USERNAME |
    CREDUI_FLAGS_ALWAYS_SHOW_UI |
    CREDUI_FLAGS_DO_NOT_PERSIST;

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

  DISALLOW_COPY_AND_ASSIGN(CredentialBufferValidator);
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
  HANDLE token;

  strcpy_s(source.SourceName, base::size(source.SourceName), "Chrome");
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

// TODO(crbug.com/574581) Remove this feature once this is confirmed to work
// as expected.
const base::Feature kCredUIPromptForWindowsCredentialsFeature{
    "CredUIPromptForWindowsCredentials", base::FEATURE_ENABLED_BY_DEFAULT};

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

  base::Time changed = base::Time::Now() - base::TimeDelta::FromSeconds(age);

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
    base::ScopedThreadMayLoadLibraryOnBackgroundThread priority_boost(
        FROM_HERE);

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

void GetOsPasswordStatusInternal(PasswordCheckPrefs* prefs,
                                 OsPasswordStatus* status) {
  DWORD username_length = CREDUI_MAX_USERNAME_LENGTH;
  WCHAR username[CREDUI_MAX_USERNAME_LENGTH+1] = {};
  *status = PASSWORD_STATUS_UNKNOWN;

  if (GetUserNameEx(NameUserPrincipal, username, &username_length)) {
    // If we are on a domain, it is almost certain that the password is not
    // blank, but we do not actively check any further than this to avoid any
    // failed login attempts hitting the domain controller.
    *status = PASSWORD_STATUS_WIN_DOMAIN;
  } else {
    username_length = CREDUI_MAX_USERNAME_LENGTH;
    if (GetUserName(username, &username_length)) {
      *status = CheckBlankPasswordWithPrefs(username, prefs) ?
          PASSWORD_STATUS_BLANK :
          PASSWORD_STATUS_NONBLANK;
    }
  }
}

void ReplyOsPasswordStatus(std::unique_ptr<PasswordCheckPrefs> prefs,
                           std::unique_ptr<OsPasswordStatus> status) {
  PrefService* local_state = g_browser_process->local_state();
  prefs->Write(local_state);
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.OsPasswordStatus", *status,
                            MAX_PASSWORD_STATUS);
}

void GetOsPasswordStatus() {
  // Preferences can be accessed on the UI thread only.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrefService* local_state = g_browser_process->local_state();
  std::unique_ptr<PasswordCheckPrefs> prefs(new PasswordCheckPrefs);
  prefs->Read(local_state);
  std::unique_ptr<OsPasswordStatus> status(
      new OsPasswordStatus(PASSWORD_STATUS_UNKNOWN));
  PasswordCheckPrefs* prefs_weak = prefs.get();
  OsPasswordStatus* status_weak = status.get();
  // This task calls ::LogonUser(), hence MayBlock().
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&GetOsPasswordStatusInternal, prefs_weak, status_weak),
      base::Bind(&ReplyOsPasswordStatus, base::Passed(&prefs),
                 base::Passed(&status)));
}

// Authenticate the user using the old Windows credential prompt.
// TODO(crbug.com/574581) Remove this feature once this is confirmed to work
// as expected.
bool AuthenticateUserOld(gfx::NativeWindow window,
                         password_manager::ReauthPurpose purpose) {
  bool retval = false;
  CREDUI_INFO cui = {};
  WCHAR username[CREDUI_MAX_USERNAME_LENGTH+1] = {};
  WCHAR displayname[CREDUI_MAX_USERNAME_LENGTH+1] = {};
  WCHAR password[CREDUI_MAX_PASSWORD_LENGTH+1] = {};
  DWORD username_length = CREDUI_MAX_USERNAME_LENGTH;
  base::string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  base::string16 password_prompt;
  switch (purpose) {
    case password_manager::ReauthPurpose::VIEW_PASSWORD:
      password_prompt =
          l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
      break;
    case password_manager::ReauthPurpose::EXPORT:
      password_prompt = l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EXPORT_AUTHENTICATION_PROMPT);
      break;
  }
  HANDLE handle = INVALID_HANDLE_VALUE;
  size_t tries = 0;
  bool use_displayname = false;
  bool use_principalname = false;
  DWORD logon_result = 0;

  // On a domain, we obtain the User Principal Name
  // for domain authentication.
  if (GetUserNameEx(NameUserPrincipal, username, &username_length)) {
    use_principalname = true;
  } else {
    username_length = CREDUI_MAX_USERNAME_LENGTH;
    // Otherwise, we're a workstation, use the plain local username.
    if (!GetUserName(username, &username_length)) {
      DLOG(ERROR) << "Unable to obtain username " << GetLastError();
      return false;
    } else {
      // As we are on a workstation, it's possible the user
      // has no password, so check here.
      if (CheckBlankPassword(username))
        return true;
    }
  }

  // Try and obtain a friendly display name.
  username_length = CREDUI_MAX_USERNAME_LENGTH;
  if (GetUserNameEx(NameDisplay, displayname, &username_length))
    use_displayname = true;

  cui.cbSize = sizeof(CREDUI_INFO);
  cui.hwndParent = NULL;
  cui.hwndParent = window->GetHost()->GetAcceleratedWidget();

  cui.pszMessageText = password_prompt.c_str();
  cui.pszCaptionText = product_name.c_str();

  cui.hbmBanner = NULL;
  BOOL save_password = FALSE;
  DWORD credErr = NO_ERROR;

  do {
    tries++;

    // TODO(wfh) Make sure we support smart cards here.
    credErr = CredUIPromptForCredentials(
        &cui,
        product_name.c_str(),
        NULL,
        0,
        use_displayname ? displayname : username,
        CREDUI_MAX_USERNAME_LENGTH+1,
        password,
        CREDUI_MAX_PASSWORD_LENGTH+1,
        &save_password,
        kCredUiDefaultFlags |
        (tries > 1 ? CREDUI_FLAGS_INCORRECT_PASSWORD : 0));

    if (credErr == NO_ERROR) {
      logon_result = LogonUser(username,
                               use_principalname ? NULL : L".",
                               password,
                               LOGON32_LOGON_INTERACTIVE,
                               LOGON32_PROVIDER_DEFAULT,
                               &handle);
      if (logon_result) {
        retval = true;
        CloseHandle(handle);
      } else {
        if (GetLastError() == ERROR_ACCOUNT_RESTRICTION &&
            wcslen(password) == 0) {
          // Password is blank, so permit.
          retval = true;
        } else {
          DLOG(WARNING) << "Unable to authenticate " << GetLastError();
        }
      }
      SecureZeroMemory(password, sizeof(password));
    }
  } while (credErr == NO_ERROR &&
           (retval == false && tries < kMaxPasswordRetries));
  return retval;
}

// Authenticate the user using the new Windows credential prompt.  The new
// prompt allows the user to authenticate using additional credential providers,
// such as PINs, smartcards, fingerprint scanners, and so on.  It also still
// allows the user to authenticate with their password.  This old prompt only
// supported password authentication which is not enough for enterprise
// environments.
bool AuthenticateUserNew(gfx::NativeWindow window,
                         password_manager::ReauthPurpose purpose) {
  bool retval = false;
  WCHAR cur_username[CREDUI_MAX_USERNAME_LENGTH + 1] = {};
  DWORD cur_username_length = base::size(cur_username);

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
  base::string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  base::string16 password_prompt;
  switch (purpose) {
    case password_manager::ReauthPurpose::VIEW_PASSWORD:
      password_prompt =
          l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
      break;
    case password_manager::ReauthPurpose::EXPORT:
      password_prompt = l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EXPORT_AUTHENTICATION_PROMPT);
      break;
  }
  CREDUI_INFO cui;
  cui.cbSize = sizeof(cui);
  cui.hwndParent = window->GetHost()->GetAcceleratedWidget();
  cui.pszMessageText = password_prompt.c_str();
  cui.pszCaptionText = product_name.c_str();
  cui.hbmBanner = nullptr;

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

}  // namespace

void DelayReportOsPassword() {
  base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                        base::BindOnce(&GetOsPasswordStatus),
                        base::TimeDelta::FromSeconds(40));
}

bool AuthenticateUser(gfx::NativeWindow window,
                      password_manager::ReauthPurpose purpose) {
  return base::FeatureList::IsEnabled(kCredUIPromptForWindowsCredentialsFeature)
             ? AuthenticateUserNew(window, purpose)
             : AuthenticateUserOld(window, purpose);
}

}  // namespace password_manager_util_win
