// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_PREFERENCES_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_PREFERENCES_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockDemoPreferencesScreen : public DemoPreferencesScreen {
 public:
  MockDemoPreferencesScreen(DemoPreferencesScreenView* view,
                            const ScreenExitCallback& exit_callback);
  ~MockDemoPreferencesScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemoPreferencesScreen);
};

class MockDemoPreferencesScreenView : public DemoPreferencesScreenView {
 public:
  MockDemoPreferencesScreenView();
  ~MockDemoPreferencesScreenView() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, MockBind, (DemoPreferencesScreen * screen));
  MOCK_METHOD(void, SetInputMethodId, (const std::string& input_method));

  void Bind(DemoPreferencesScreen* screen) override;

 private:
  DemoPreferencesScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockDemoPreferencesScreenView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_PREFERENCES_SCREEN_H_
