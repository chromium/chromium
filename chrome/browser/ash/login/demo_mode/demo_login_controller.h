// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chromeos/ash/experiences/login/login_screen_shown_observer.h"
#include "services/network/public/cpp/simple_url_loader.h"

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
    kNetworkError = 4,          // Network error.
    kRequestFailed = 5,         // Server side error or out of quota.
  };

  using FailedRequestCallback =
      base::OnceCallback<void(const ResultCode result_code)>;

  explicit DemoLoginController(LoginScreenClientImpl* login_screen_client);
  DemoLoginController(const DemoLoginController&) = delete;
  DemoLoginController& operator=(const DemoLoginController&) = delete;
  ~DemoLoginController() override;

  // LoginScreenShownObserver:
  void OnLoginScreenShown() override;

  void SetSetupFailedCallbackForTest(
      base::OnceCallback<void(const ResultCode result_code)> callback);
  void SetCleanUpFailedCallbackForTest(
      base::OnceCallback<void(const ResultCode result_code)> callback);

 private:
  // Maybe send clean up request to clean up account used in last session if
  // presents.
  void MaybeCleanupPreviousDemoAccount();

  // Sends a request to create Demo accounts and login with this account.
  void SendSetupDemoAccountRequest();
  // Called on setup demo account complete.
  void OnSetupDemoAccountComplete(const std::string& device_id,
                                  std::unique_ptr<std::string> response_body);
  // Parses the setup demo account response body and maybe login demo account.
  void HandleSetupDemoAcountResponse(
      const std::string& device_id,
      const std::unique_ptr<std::string> response_body);

  void OnSetupDemoAccountError(const ResultCode result_code);

  // Called on clean up demo account complete.
  void OnCleanUpDemoAccountComplete(std::unique_ptr<std::string> response_body);

  // We only allow 1 demo account request at a time.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  FailedRequestCallback setup_failed_callback_for_testing_;
  FailedRequestCallback clean_up_failed_callback_for_testing_;

  base::ScopedObservation<LoginScreenClientImpl, LoginScreenShownObserver>
      scoped_observation_{this};

  base::WeakPtrFactory<DemoLoginController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_
