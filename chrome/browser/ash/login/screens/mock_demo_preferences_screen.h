// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_PREFERENCES_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_PREFERENCES_SCREEN_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockDemoPreferencesScreen : public DemoPreferencesScreen {
 public:
  MockDemoPreferencesScreen(base::WeakPtr<DemoPreferencesScreenView> view,
                            const ScreenExitCallback& exit_callback);

  MockDemoPreferencesScreen(const MockDemoPreferencesScreen&) = delete;
  MockDemoPreferencesScreen& operator=(const MockDemoPreferencesScreen&) =
      delete;

  ~MockDemoPreferencesScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);
};

class MockDemoPreferencesScreenView final : public DemoPreferencesScreenView {
 public:
  MockDemoPreferencesScreenView();

  MockDemoPreferencesScreenView(const MockDemoPreferencesScreenView&) = delete;
  MockDemoPreferencesScreenView& operator=(
      const MockDemoPreferencesScreenView&) = delete;

  ~MockDemoPreferencesScreenView() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, SetInputMethodId, (const std::string& input_method));

  base::WeakPtr<DemoPreferencesScreenView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<DemoPreferencesScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_PREFERENCES_SCREEN_H_
