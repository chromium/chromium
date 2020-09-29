// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_persistence.h"

#include "base/logging.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/login_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "ui/base/ime/chromeos/input_method_util.h"

namespace chromeos {
namespace input_method {
namespace {

void PersistSystemInputMethod(const std::string& input_method) {
  if (!g_browser_process || !g_browser_process->local_state())
    return;

  g_browser_process->local_state()->SetString(
        language_prefs::kPreferredKeyboardLayout, input_method);
}

// Returns the user email, whether or not they have consented to browser sync.
AccountId GetUserAccount(Profile* profile) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return EmptyAccountId();
  return user->GetAccountId();
}

static void SetUserLastInputMethodPreference(const AccountId& account_id,
                                             const std::string& input_method) {
  if (!account_id.is_valid())
    return;
  user_manager::known_user::SetUserLastInputMethod(account_id, input_method);
}

// Update user Last keyboard layout for login screen
static void SetUserLastInputMethod(
    const std::string& input_method,
    const chromeos::input_method::InputMethodManager* const manager,
    Profile* profile) {
  // Skip if it's not a keyboard layout. Drop input methods including
  // extension ones.
  if (!manager->IsLoginKeyboard(input_method))
    return;

  if (profile == NULL)
    return;

  // TODO(https://crbug.com/1121565): Create more general fix for all the data
  // that is required on the lock screen.
  profile->GetPrefs()->SetString(prefs::kLastLoginInputMethod, input_method);
  SetUserLastInputMethodPreference(GetUserAccount(profile), input_method);
}

void PersistUserInputMethod(const std::string& input_method,
                            InputMethodManager* const manager,
                            Profile* profile) {
  PrefService* user_prefs = NULL;
  // Persist the method on a per user basis. Note that the keyboard settings are
  // stored per user desktop and a visiting window will use the same input
  // method as the desktop it is on (and not of the owner of the window).
  if (profile)
    user_prefs = profile->GetPrefs();
  if (!user_prefs)
    return;
  SetUserLastInputMethod(input_method, manager, profile);

  const std::string current_input_method_on_pref =
      user_prefs->GetString(::prefs::kLanguageCurrentInputMethod);
  if (current_input_method_on_pref == input_method)
    return;

  user_prefs->SetString(::prefs::kLanguagePreviousInputMethod,
                        current_input_method_on_pref);
  user_prefs->SetString(::prefs::kLanguageCurrentInputMethod, input_method);
}

}  // namespace

InputMethodPersistence::InputMethodPersistence(
    InputMethodManager* input_method_manager)
    : input_method_manager_(input_method_manager),
      ui_session_(InputMethodManager::STATE_LOGIN_SCREEN) {
  input_method_manager_->AddObserver(this);
}

InputMethodPersistence::~InputMethodPersistence() {
  input_method_manager_->RemoveObserver(this);
}

void InputMethodPersistence::InputMethodChanged(InputMethodManager* manager,
                                                Profile* profile,
                                                bool show_message) {
  DCHECK_EQ(input_method_manager_, manager);
  // We might get here during the locking process. When locker is already
  // created but session state has not changed yet.
  if (ScreenLocker::default_screen_locker()) {
    // We use a special set of input methods on the lock screen. Do not update.
    return;
  }
  const std::string current_input_method =
      manager->GetActiveIMEState()->GetCurrentInputMethod().id();
  // Save the new input method id depending on the current browser state.
  switch (ui_session_) {
    case InputMethodManager::STATE_LOGIN_SCREEN:
      if (!manager->IsLoginKeyboard(current_input_method)) {
        DVLOG(1) << "Only keyboard layouts are supported: "
                 << current_input_method;
        return;
      }
      PersistSystemInputMethod(current_input_method);
      return;
    case InputMethodManager::STATE_BROWSER_SCREEN:
      PersistUserInputMethod(current_input_method, manager, profile);
      return;
    case InputMethodManager::STATE_LOCK_SCREEN:
    case InputMethodManager::STATE_SECONDARY_LOGIN_SCREEN:
      // We use a special set of input methods on the screen. Do not update.
      return;
    case InputMethodManager::STATE_TERMINATING:
      return;
  }
  NOTREACHED();
}

void InputMethodPersistence::OnSessionStateChange(
    InputMethodManager::UISessionState new_ui_session) {
  InputMethodManager::UISessionState previous_ui_session = ui_session_;
  ui_session_ = new_ui_session;

  // Persist input method when transitioning from Login screen into the session.
  if (previous_ui_session == InputMethodManager::STATE_LOGIN_SCREEN &&
      ui_session_ == InputMethodManager::STATE_BROWSER_SCREEN) {
    const std::string current_input_method =
        input_method_manager_->GetActiveIMEState()
            ->GetCurrentInputMethod()
            .id();
    SetUserLastInputMethod(current_input_method, input_method_manager_,
                           ProfileManager::GetActiveUserProfile());
  }
}

void SetUserLastInputMethodPreferenceForTesting(
    const AccountId& account_id,
    const std::string& input_method) {
  SetUserLastInputMethodPreference(account_id, input_method);
}

}  // namespace input_method
}  // namespace chromeos
