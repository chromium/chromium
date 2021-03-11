// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_

#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUpdateScreen : public UpdateScreen {
 public:
  MockUpdateScreen(UpdateView* view,
                   ErrorScreen* error_screen,
                   const ScreenExitCallback& exit_callback);
  virtual ~MockUpdateScreen();

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void RunExit(UpdateScreen::Result result);
};

class MockUpdateView : public UpdateView {
 public:
  MockUpdateView();
  virtual ~MockUpdateView();

  void Bind(UpdateScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, MockBind, (UpdateScreen * screen));
  MOCK_METHOD(void, MockUnbind, ());

  MOCK_METHOD(void, SetUpdateState, (UpdateView::UIState value));
  MOCK_METHOD(void,
              SetUpdateStatus,
              (int percent,
               const std::u16string& percent_message,
               const std::u16string& timeleft_message));
  MOCK_METHOD(void, SetEstimatedTimeLeft, (int value));
  MOCK_METHOD(void, SetShowEstimatedTimeLeft, (bool value));
  MOCK_METHOD(void, SetUpdateCompleted, (bool value));
  MOCK_METHOD(void, SetShowCurtain, (bool value));
  MOCK_METHOD(void, SetProgressMessage, (const std::u16string& value));
  MOCK_METHOD(void, SetProgress, (int value));
  MOCK_METHOD(void, SetCancelUpdateShortcutEnabled, (bool value));
  MOCK_METHOD(void, ShowLowBatteryWarningMessage, (bool value));
  MOCK_METHOD(void, SetAutoTransition, (bool value));

 private:
  UpdateScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_
