// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockUpdateScreen : public UpdateScreen {
 public:
  MockUpdateScreen(base::WeakPtr<UpdateView> view,
                   ErrorScreen* error_screen,
                   const ScreenExitCallback& exit_callback);
  ~MockUpdateScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void RunExit(UpdateScreen::Result result);
};

class MockUpdateView final : public UpdateView {
 public:
  MockUpdateView();
  ~MockUpdateView() override;

  MOCK_METHOD(void, Show, (bool is_opt_out_enabled));

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

  base::WeakPtr<UpdateView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<UpdateView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_UPDATE_SCREEN_H_
