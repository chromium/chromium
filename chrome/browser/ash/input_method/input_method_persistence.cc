// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_persistence.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/language_preferences/language_preferences.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash::input_method {
namespace {

InputMethodPersistence* g_input_method_persistence = nullptr;

void PersistSystemInputMethod(PrefService& local_state,
                              const std::string& input_method) {
  local_state.SetString(language_prefs::kPreferredKeyboardLayout, input_method);
}

// Returns the user email, whether or not they have consented to browser sync.
AccountId GetUserAccount(Profile* profile) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    return EmptyAccountId();
  }
  return user->GetAccountId();
}

void SetUserLastInputMethodPreference(PrefService& local_state,
                                      const AccountId& account_id,
                                      const std::string& input_method_id) {
  if (!account_id.is_valid()) {
    return;
  }
  user_manager::KnownUser known_user(&local_state);
  known_user.SetUserLastLoginInputMethodId(account_id, input_method_id);
}

}  // namespace

// static
InputMethodPersistence* InputMethodPersistence::GetInstance() {
  CHECK(g_input_method_persistence);
  return g_input_method_persistence;
}

InputMethodPersistence::InputMethodPersistence(
    PrefService* local_state,
    InputMethodManager* input_method_manager)
    : local_state_(CHECK_DEREF(local_state)),
      input_method_manager_(input_method_manager) {
  CHECK(!g_input_method_persistence);
  g_input_method_persistence = this;
}

InputMethodPersistence::~InputMethodPersistence() {
  CHECK_EQ(g_input_method_persistence, this);
  g_input_method_persistence = nullptr;
}

void InputMethodPersistence::PersistInputMethod(Profile* profile) {
  if (!g_browser_process || g_browser_process->IsShuttingDown()) {
    return;
  }

  const std::string current_input_method =
      input_method_manager_->GetActiveIMEState()->GetCurrentInputMethod().id();
  // Save the new input method id depending on the current browser state.
  switch (input_method_manager_->GetActiveIMEState()->GetUIStyle()) {
    case InputMethodManager::UIStyle::kLogin:
      if (!input_method_manager_->IsLoginKeyboard(current_input_method)) {
        DVLOG(1) << "Only keyboard layouts are supported: "
                 << current_input_method;
        return;
      }
      PersistSystemInputMethod(*local_state_, current_input_method);
      return;
    case InputMethodManager::UIStyle::kNormal:
      PersistUserInputMethod(current_input_method, profile);
      return;
    case InputMethodManager::UIStyle::kLock:
      // We are either in unit test, or screen should be locked.
      DCHECK(!LoginScreenClientImpl::HasInstance() ||
             ScreenLocker::default_screen_locker());
      return;
    case InputMethodManager::UIStyle::kSecondaryLogin:
      // We use a special set of input methods on the screen. Do not update.
      return;
  }
  NOTREACHED();
}

void InputMethodPersistence::SetUserLastLoginInputMethodId(
    const std::string& input_method_id,
    Profile* profile) {
  if (!profile) {
    return;
  }

  // Skip if it's not a keyboard layout. Drop input methods including
  // extension ones.
  if (!input_method_manager_->IsLoginKeyboard(input_method_id)) {
    return;
  }

  // TODO(https://crbug.com/1121565): Create more general fix for all the data
  // that is required on the lock screen.
  profile->GetPrefs()->SetString(prefs::kLastLoginInputMethod, input_method_id);
  SetUserLastInputMethodPreference(*local_state_, GetUserAccount(profile),
                                   input_method_id);
}

void InputMethodPersistence::PersistUserInputMethod(
    const std::string& input_method_id,
    Profile* profile) {
  PrefService* user_prefs = nullptr;
  // Persist the method on a per user basis. Note that the keyboard settings are
  // stored per user desktop and a visiting window will use the same input
  // method as the desktop it is on (and not of the owner of the window).
  if (profile) {
    user_prefs = profile->GetPrefs();
  }
  if (!user_prefs) {
    return;
  }

  SetUserLastLoginInputMethodId(input_method_id, profile);

  const std::string current_input_method_id_on_pref =
      user_prefs->GetString(::prefs::kLanguageCurrentInputMethod);
  if (current_input_method_id_on_pref == input_method_id) {
    return;
  }

  user_prefs->SetString(::prefs::kLanguagePreviousInputMethod,
                        current_input_method_id_on_pref);
  user_prefs->SetString(::prefs::kLanguageCurrentInputMethod, input_method_id);
}

// static
void InputMethodPersistence::SetUserLastInputMethodPreferenceForTesting(
    PrefService& local_state,
    const AccountId& account_id,
    const std::string& input_method) {
  SetUserLastInputMethodPreference(local_state, account_id, input_method);
}

}  // namespace ash::input_method
