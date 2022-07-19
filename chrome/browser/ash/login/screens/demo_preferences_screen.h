// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

// Controls demo mode preferences. The screen can be shown during OOBE. It
// allows user to choose preferences for retail demo mode.
class DemoPreferencesScreen
    : public BaseScreen,
      public input_method::InputMethodManager::Observer {
 public:
  enum class Result { COMPLETED, COMPLETED_CONSOLIDATED_CONSENT, CANCELED };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  DemoPreferencesScreen(base::WeakPtr<DemoPreferencesScreenView> view,
                        const ScreenExitCallback& exit_callback);

  DemoPreferencesScreen(const DemoPreferencesScreen&) = delete;
  DemoPreferencesScreen& operator=(const DemoPreferencesScreen&) = delete;

  ~DemoPreferencesScreen() override;

  void SetDemoModeCountry(const std::string& country_id);
  void SetDemoModeRetailerAndStoreIdInput(
      const std::string& retailer_store_id_input);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  // InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // Passes current input method to the context, so it can be shown in the UI.
  void UpdateInputMethod(input_method::InputMethodManager* input_manager);

  base::ScopedObservation<input_method::InputMethodManager,
                          input_method::InputMethodManager::Observer>
      input_manager_observation_{this};

  base::WeakPtr<DemoPreferencesScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::DemoPreferencesScreen;
}

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::DemoPreferencesScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_
