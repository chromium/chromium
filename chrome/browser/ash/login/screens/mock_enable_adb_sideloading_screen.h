// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockEnableAdbSideloadingScreen : public EnableAdbSideloadingScreen {
 public:
  MockEnableAdbSideloadingScreen(
      base::WeakPtr<EnableAdbSideloadingScreenView> view,
      const base::RepeatingClosure& exit_callback);
  ~MockEnableAdbSideloadingScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());
  MOCK_METHOD(void, OnUserAction, (const base::Value::List&));

  void ExitScreen();
};

class MockEnableAdbSideloadingScreenView final
    : public EnableAdbSideloadingScreenView {
 public:
  MockEnableAdbSideloadingScreenView();
  ~MockEnableAdbSideloadingScreenView() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, SetScreenState, (UIState value));

  base::WeakPtr<EnableAdbSideloadingScreenView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<EnableAdbSideloadingScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_
