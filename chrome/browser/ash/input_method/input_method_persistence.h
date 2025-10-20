// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ash/input_method_manager.h"

class AccountId;

namespace ash {
namespace input_method {

// Observes input method and session state changes, and persists input method
// changes to the BrowserProcess local state or to the user preferences,
// according to the session state.
class InputMethodPersistence : public InputMethodManager::Observer {
 public:
  // Constructs an instance that will observe input method changes on the
  // provided InputMethodManager. The client is responsible for calling
  // OnSessionStateChange whenever the InputMethodManager::UISessionState
  // changes.
  explicit InputMethodPersistence(InputMethodManager* input_method_manager);

  InputMethodPersistence(const InputMethodPersistence&) = delete;
  InputMethodPersistence& operator=(const InputMethodPersistence&) = delete;

  ~InputMethodPersistence() override;

  // This method does not instantiate the object. It must be called after
  // ash::input_method::Initialize();
  static InputMethodPersistence* GetInstance();

  // InputMethodManager::Observer overrides.
  void InputMethodChanged(InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // Update user last input method ID for login screen.
  void SetUserLastLoginInputMethodId(const std::string& input_method_id,
                                     Profile* profile);

  // Update user last input method ID into known_user data.
  static void SetUserLastInputMethodPreferenceForTesting(
      const AccountId& account_id,
      const std::string& input_method);

 private:
  void PersistUserInputMethod(const std::string& input_method_id,
                              Profile* profile);

  raw_ptr<InputMethodManager> input_method_manager_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_
