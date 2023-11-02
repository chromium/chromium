// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_MOCK_NETWORK_STATE_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_MOCK_NETWORK_STATE_HELPER_H_

#include "chrome/browser/ash/login/helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace login {

class MockNetworkStateHelper : public NetworkStateHelper {
 public:
  MockNetworkStateHelper();
  ~MockNetworkStateHelper() override;
  MOCK_CONST_METHOD0(GetCurrentNetworkName, std::u16string(void));
  MOCK_CONST_METHOD0(IsConnected, bool(void));
  MOCK_CONST_METHOD0(IsConnectedToEthernet, bool(void));
  MOCK_CONST_METHOD0(IsConnecting, bool(void));
};

}  // namespace login
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_MOCK_NETWORK_STATE_HELPER_H_
