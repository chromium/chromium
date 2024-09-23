// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_DEBUGGING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_DEBUGGING_SCREEN_H_

#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockEnableDebuggingScreen : public EnableDebuggingScreen {
 public:
  MockEnableDebuggingScreen(base::WeakPtr<EnableDebuggingScreenView> view,
                            const base::RepeatingClosure& exit_callback);
  ~MockEnableDebuggingScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen();
};

class MockEnableDebuggingScreenView final : public EnableDebuggingScreenView {
 public:
  MockEnableDebuggingScreenView();
  ~MockEnableDebuggingScreenView() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, UpdateUIState, (UIState state));

  base::WeakPtr<EnableDebuggingScreenView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<EnableDebuggingScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_DEBUGGING_SCREEN_H_
