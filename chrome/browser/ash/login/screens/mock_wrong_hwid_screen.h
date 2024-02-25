// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WRONG_HWID_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WRONG_HWID_SCREEN_H_

#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/ui/webui/ash/login/wrong_hwid_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockWrongHWIDScreen : public WrongHWIDScreen {
 public:
  MockWrongHWIDScreen(base::WeakPtr<WrongHWIDScreenView> view,
                      const base::RepeatingClosure& exit_callback);
  ~MockWrongHWIDScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen();
};

class MockWrongHWIDScreenView final : public WrongHWIDScreenView {
 public:
  MockWrongHWIDScreenView();
  ~MockWrongHWIDScreenView() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());

  base::WeakPtr<WrongHWIDScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<WrongHWIDScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WRONG_HWID_SCREEN_H_
