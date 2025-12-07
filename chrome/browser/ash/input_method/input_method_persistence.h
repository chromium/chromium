// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

class AccountId;
class PrefService;
class Profile;

namespace ash {
namespace input_method {

class InputMethodManager;

// Handles input method and session state changes, and persists input method
// changes to the BrowserProcess local state or to the user preferences,
// according to the session state.
class InputMethodPersistence {
 public:
  // Constructs an instance that will react to input method changes on the
  // provided InputMethodManager. The client is responsible for calling
  // OnSessionStateChange whenever the InputMethodManager::UISessionState
  // changes.
  //
  // `local_state` must be non-null, and must outlive `this`.
  InputMethodPersistence(PrefService* local_state,
                         InputMethodManager* input_method_manager);

  InputMethodPersistence(const InputMethodPersistence&) = delete;
  InputMethodPersistence& operator=(const InputMethodPersistence&) = delete;

  ~InputMethodPersistence();

  // This method does not instantiate the object. It must be called after
  // ash::input_method::Initialize();
  static InputMethodPersistence* GetInstance();

  // Called when the current input method is changed.
  void PersistInputMethod(Profile* profile);

  // Update user last input method ID for login screen.
  void SetUserLastLoginInputMethodId(const std::string& input_method_id,
                                     Profile* profile);

  // Update user last input method ID into known_user data.
  static void SetUserLastInputMethodPreferenceForTesting(
      PrefService& local_state,
      const AccountId& account_id,
      const std::string& input_method);

 private:
  void PersistUserInputMethod(const std::string& input_method_id,
                              Profile* profile);

  const raw_ref<PrefService> local_state_;

  raw_ptr<InputMethodManager> input_method_manager_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_
