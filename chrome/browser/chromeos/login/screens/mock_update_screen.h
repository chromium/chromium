// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUpdateScreen : public UpdateScreen {
 public:
  MockUpdateScreen(UpdateView* view,
                   ErrorScreen* error_screen,
                   const ScreenExitCallback& exit_callback);
  virtual ~MockUpdateScreen();

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  void RunExit(UpdateScreen::Result result);
};

class MockUpdateView : public UpdateView {
 public:
  MockUpdateView();
  virtual ~MockUpdateView();

  void Bind(UpdateScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(MockBind, void(UpdateScreen* screen));
  MOCK_METHOD0(MockUnbind, void());

  MOCK_METHOD1(SetEstimatedTimeLeft, void(int value));
  MOCK_METHOD1(SetShowEstimatedTimeLeft, void(bool value));
  MOCK_METHOD1(SetUpdateCompleted, void(bool value));
  MOCK_METHOD1(SetShowCurtain, void(bool value));
  MOCK_METHOD1(SetProgressMessage, void(const base::string16& value));
  MOCK_METHOD1(SetProgress, void(int value));
  MOCK_METHOD1(SetRequiresPermissionForCellular, void(bool value));
  MOCK_METHOD1(SetCancelUpdateShortcutEnabled, void(bool value));

 private:
  UpdateScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_
