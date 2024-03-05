// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockWelcomeScreen : public WelcomeScreen {
 public:
  MockWelcomeScreen(base::WeakPtr<WelcomeView> view,
                    const WelcomeScreen::ScreenExitCallback& exit_callback);

  MockWelcomeScreen(const MockWelcomeScreen&) = delete;
  MockWelcomeScreen& operator=(const MockWelcomeScreen&) = delete;

  ~MockWelcomeScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);
};

class MockWelcomeView final : public WelcomeView {
 public:
  MockWelcomeView();

  MockWelcomeView(const MockWelcomeView&) = delete;
  MockWelcomeView& operator=(const MockWelcomeView&) = delete;

  ~MockWelcomeView() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, SetLanguageList, (base::Value::List));
  MOCK_METHOD(void, SetInputMethodId, (const std::string& input_method_id));
  MOCK_METHOD(void, SetTimezoneId, (const std::string& timezone_id));
  MOCK_METHOD(void, ShowDemoModeConfirmationDialog, ());
  MOCK_METHOD(void, ShowEditRequisitionDialog, (const std::string&));
  MOCK_METHOD(void, ShowRemoraRequisitionDialog, ());
  MOCK_METHOD(void, GiveChromeVoxHint, ());
  MOCK_METHOD(void, CancelChromeVoxHintIdleDetection, ());
  MOCK_METHOD(void, UpdateA11yState, (const A11yState&));
  MOCK_METHOD(void, SetQuickStartEnabled, ());

  base::WeakPtr<WelcomeView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<WelcomeView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_
