// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_

#include <optional>
#include <string>

#include "ash/metrics/demo_session_metrics_recorder.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ash {

// Manage Demo accounts life cycle for Demo mode. Handle demo accounts setup and
// clean up.
class DemoLoginController
    : public policy::DeviceCloudPolicyManagerAsh::Observer {
 public:
  enum class State {
    // `DemoLoginController` is not initialized properly.
    kUnknown = 0,

    // Loading feature from policy/flag/growth in progress.
    kLoadingAvailibility = 1,

    // Finish loading feature from policy/flag/growth. The feature is enabled
    // and ready to login demo account.
    kReadyForLoginWithDemoAccount = 2,

    // The feature is enabled and setting up demo account is in progress
    kSetupDemoAccountInProgress = 3,

    // The feature is enabled and demo account is setup. Logging in demo
    // account.
    kLoginDemoAccount = 4,

    // The feature is disabled. Or feature is enabled but setting up demo
    // account fails. Logging in MGS.
    kLoginToMGS = 5,
  };

  using RequestCallback = base::OnceCallback<void()>;

  using ResultCode = DemoSessionMetricsRecorder::DemoAccountRequestResultCode;

  explicit DemoLoginController(base::RepeatingClosure auto_login_mgs_callback);
  DemoLoginController(const DemoLoginController&) = delete;
  DemoLoginController& operator=(const DemoLoginController&) = delete;
  ~DemoLoginController() override;

  State state() const { return state_; }

  // Trigger Demo account login flow.
  void TriggerDemoAccountLoginFlow();

  void SetSetupRequestCallbackForTesting(RequestCallback callback);
  void SetCleanupRequestCallbackForTesting(RequestCallback callback);
  void SetDeviceCloudPolicyManagerForTesting(
      policy::CloudPolicyManager* policy_manager);

 private:
  // policy::DeviceCloudPolicyManagerAsh::Observer:
  void OnDeviceCloudPolicyManagerConnected() override;
  void OnDeviceCloudPolicyManagerGotRegistry() override;

  // Call if feature `GrowthCampaignsDemoModeSignIn` is enable but feature
  // `DemoModeSignIn` not enabled.
  void LoadFeatureEligibilityFromGrowth();

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

  void OnCleanUpDemoAccountError(const ResultCode result_code);

  // We keep this function in-class because it needs to access the member
  // `policy_manager_for_testing_`, which is set by unit tests through
  // SetDeviceCloudPolicyManagerForTesting().
  std::optional<base::Value::Dict> GetDeviceIdentifier(
      const std::string& login_scope_device_id);

  // Called on the feature is finished loading from growth.
  void HandleFeatureEligibility(bool is_sign_in_enable);

  // Called on growth campaign is loaded, parse campaign result.
  void OnCampaignsLoaded();

  // Call to update `state_` from State:kLoading to next state and maybe trigger
  // auto login.
  void MaybeTriggerAutoLogin();

  // Called on 5th second for waiting policy manager connection.
  void OnPolicyManagerConnectionTimeOut();

  // We only allow 1 demo account request at a time.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Callback to auto login to manage guest session or trigger demo account
  // login.
  base::RepeatingClosure configure_auto_login_callback_;

  State state_ = State::kUnknown;

  // Determine whether `State::kLoading` is finished (when both are `true`).
  bool is_policy_manager_connected_ = false;
  bool is_feature_eligiblity_loaded_ = false;

  // If true, the cloud policy connection is not available. Fallback to MGS.
  bool is_loading_policy_manager_timeout_ = false;

  RequestCallback setup_request_callback_for_testing_;
  RequestCallback cleanup_request_callback_for_testing_;

  raw_ptr<policy::CloudPolicyManager> policy_manager_for_testing_ = nullptr;

  base::ScopedObservation<policy::DeviceCloudPolicyManagerAsh,
                          policy::DeviceCloudPolicyManagerAsh::Observer>
      observation_{this};

  // WeakPtrFactory members which refer to their outer class must be the last
  // member in the outer class definition.
  base::WeakPtrFactory<DemoLoginController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_LOGIN_CONTROLLER_H_
