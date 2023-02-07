// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class DemoPreferencesScreenView;

// Controls demo mode preferences. The screen can be shown during OOBE. It
// allows user to choose preferences for retail demo mode.
class DemoPreferencesScreen : public BaseScreen {
 public:
  enum class Result { COMPLETED, CANCELED };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  DemoPreferencesScreen(base::WeakPtr<DemoPreferencesScreenView> view,
                        const ScreenExitCallback& exit_callback);

  DemoPreferencesScreen(const DemoPreferencesScreen&) = delete;
  DemoPreferencesScreen& operator=(const DemoPreferencesScreen&) = delete;

  ~DemoPreferencesScreen() override;

  void SetDemoModeCountry(const std::string& country_id);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  base::WeakPtr<DemoPreferencesScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_
