// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_

#include "base/macros.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockNetworkScreen : public NetworkScreen {
 public:
  MockNetworkScreen(NetworkScreenView* view,
                    const ScreenExitCallback& exit_callback);
  ~MockNetworkScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(NetworkScreen::Result result);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetworkScreen);
};

class MockNetworkScreenView : public NetworkScreenView {
 public:
  MockNetworkScreenView();
  ~MockNetworkScreenView() override;

  void Bind(NetworkScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD(void, MockBind, (NetworkScreen * screen));
  MOCK_METHOD(void, MockUnbind, ());
  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, ShowError, (const std::u16string& message));
  MOCK_METHOD(void, ClearErrors, ());
  MOCK_METHOD(void,
              ShowConnectingStatus,
              (bool connecting, const std::u16string& network_id));
  MOCK_METHOD(void, SetOfflineDemoModeEnabled, (bool enabled));

 private:
  NetworkScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockNetworkScreenView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_
