// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_

#include "chrome/browser/ash/login/screens/eula_screen.h"
#include "chrome/browser/ui/webui/ash/login/eula_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockEulaScreen : public EulaScreen {
 public:
  MockEulaScreen(base::WeakPtr<EulaView> view,
                 const ScreenExitCallback& exit_callback);
  ~MockEulaScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);
};

class MockEulaView : public EulaView {
 public:
  MockEulaView();
  ~MockEulaView() override;

  MOCK_METHOD(void, Show, (const bool is_cloud_ready_update_flow), (override));
  MOCK_METHOD(void, Hide, (), (override));

  MOCK_METHOD(void, SetUsageStatsEnabled, (bool enabled), (override));
  MOCK_METHOD(void, ShowStatsUsageLearnMore, (), (override));
  MOCK_METHOD(void, ShowAdditionalTosDialog, (), (override));
  MOCK_METHOD(void, ShowSecuritySettingsDialog, (), (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_
