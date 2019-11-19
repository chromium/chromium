// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_adb_sideloading_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockEnableAdbSideloadingScreen : public EnableAdbSideloadingScreen {
 public:
  MockEnableAdbSideloadingScreen(EnableAdbSideloadingScreenView* view,
                                 const base::RepeatingClosure& exit_callback);
  ~MockEnableAdbSideloadingScreen() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  void ExitScreen();
};

class MockEnableAdbSideloadingScreenView
    : public EnableAdbSideloadingScreenView {
 public:
  MockEnableAdbSideloadingScreenView();
  ~MockEnableAdbSideloadingScreenView() override;

  void Bind(EnableAdbSideloadingScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(MockBind, void(EnableAdbSideloadingScreen* screen));
  MOCK_METHOD0(MockUnbind, void());
  MOCK_METHOD1(SetScreenState, void(UIState value));

 private:
  EnableAdbSideloadingScreen* screen_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_
