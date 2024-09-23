// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ERROR_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockErrorScreen : public ErrorScreen {
 public:
  explicit MockErrorScreen(base::WeakPtr<ErrorScreenView> view);
  ~MockErrorScreen() override;

  void FixCaptivePortal() override;
  void SetUIState(NetworkError::UIState ui_state) override;
  void SetErrorState(NetworkError::ErrorState error_state,
                     const std::string& network) override;

  MOCK_METHOD0(MockFixCaptivePortal, void());
  MOCK_METHOD1(MockSetUIState, void(NetworkError::UIState ui_state));
  MOCK_METHOD2(MockSetErrorState,
               void(NetworkError::ErrorState error_state,
                    const std::string& network));
};

class MockErrorScreenView : public ErrorScreenView {
 public:
  MockErrorScreenView();
  ~MockErrorScreenView() override;

  MOCK_METHOD1(ShowScreenWithParam, void(bool is_closeable));
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(ShowOobeScreen, void(OobeScreenId screen));
  MOCK_METHOD1(SetErrorStateCode, void(NetworkError::ErrorState error_state));
  MOCK_METHOD1(SetErrorStateNetwork, void(const std::string& network_name));
  MOCK_METHOD1(SetGuestSigninAllowed, void(bool value));
  MOCK_METHOD1(SetOfflineSigninAllowed, void(bool value));
  MOCK_METHOD1(SetShowConnectingIndicator, void(bool value));
  MOCK_METHOD1(SetUIState, void(NetworkError::UIState ui_state));
  MOCK_METHOD0(OnCancelButtonClicked, void());

  base::WeakPtr<ErrorScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<ErrorScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ERROR_SCREEN_H_
