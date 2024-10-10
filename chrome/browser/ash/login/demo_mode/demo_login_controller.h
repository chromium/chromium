// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_

#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chromeos/ash/experiences/login/login_screen_shown_observer.h"

namespace ash {

// Manage Demo accounts life cycle for Demo mode. Handle demo accounts setup and
// clean up.
class DemoLoginController : public LoginScreenShownObserver {
 public:
  enum class ResultCode {
    kSuccess = 0,               // Demo account request success.
    kResponseParsingError = 1,  // Malformat Http response.
    kInvalidCreds = 2,          // Missing required credential for login.
    kEmptyReponse = 3,          // Empty Http response.
  };

  explicit DemoLoginController(LoginScreenClientImpl* login_screen_client);
  DemoLoginController(const DemoLoginController&) = delete;
  DemoLoginController& operator=(const DemoLoginController&) = delete;
  ~DemoLoginController() override;

  // LoginScreenShownObserver:
  void OnLoginScreenShown() override;

  void SetSetupDemoAccountResponseForTest(
      const std::string& setup_demo_account_response);

 private:
  // Sends a request to create Demo accounts and login with this account.
  void SendSetupDemoAccountRequest();
  // Called on setup demo account complete.
  void OnSetupDemoAccountComplete();
  // Parses the setup demo account response body and maybe login demo account.
  void ParseSetupDemoAccountResponse(const std::string& response_body);

  // TODO(crbug.com/364214790): Remove once implement the `SimpleURLLoader` for
  // set up account request.
  std::string setup_demo_account_response_;

  base::ScopedObservation<LoginScreenClientImpl, LoginScreenShownObserver>
      scoped_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_
