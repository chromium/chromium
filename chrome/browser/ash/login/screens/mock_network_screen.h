// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_

#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockNetworkScreen : public NetworkScreen {
 public:
  MockNetworkScreen(base::WeakPtr<NetworkScreenView> view,
                    const ScreenExitCallback& exit_callback);

  MockNetworkScreen(const MockNetworkScreen&) = delete;
  MockNetworkScreen& operator=(const MockNetworkScreen&) = delete;

  ~MockNetworkScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(NetworkScreen::Result result);
};

class MockNetworkScreenView final : public NetworkScreenView {
 public:
  MockNetworkScreenView();

  MockNetworkScreenView(const MockNetworkScreenView&) = delete;
  MockNetworkScreenView& operator=(const MockNetworkScreenView&) = delete;

  ~MockNetworkScreenView() override;

  MOCK_METHOD(void, ShowScreenWithData, (base::Value::Dict data));
  MOCK_METHOD(void, ShowError, (const std::u16string& message));
  MOCK_METHOD(void, ClearErrors, ());
  MOCK_METHOD(void, SetOfflineDemoModeEnabled, (bool enabled));
  MOCK_METHOD(void, SetQuickStartEntryPointVisibility, (bool visible));

  base::WeakPtr<NetworkScreenView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<NetworkScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_
