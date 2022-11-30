// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/request_type.h"

class PredictionServiceRequest;
class Profile;

namespace permissions {
struct PredictionRequestFeatures;
class GeneratePredictionsResponse;
}  // namespace permissions

// Each instance of this class is long-lived and can support multiple requests,
// but only one at a time.
class PredictionBasedPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  enum class PredictionSource {
    USE_SERVER_SIDE,
    USE_ONDEVICE,
    USE_ANY,
    USE_NONE,
  };
  using PredictionGrantLikelihood =
      permissions::PermissionUmaUtil::PredictionGrantLikelihood;
  // Constructs an instance in the context of the given |profile|.
  explicit PredictionBasedPermissionUiSelector(Profile* profile);
  ~PredictionBasedPermissionUiSelector() override;

  PredictionBasedPermissionUiSelector(
      const PredictionBasedPermissionUiSelector&) = delete;
  PredictionBasedPermissionUiSelector& operator=(
      const PredictionBasedPermissionUiSelector&) = delete;

  // NotificationPermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  void Cancel() override;

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override;

  absl::optional<PredictionGrantLikelihood> PredictedGrantLikelihoodForUKM()
      override;

  absl::optional<bool> WasSelectorDecisionHeldback() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           GetPredictionTypeToUse);
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           HoldbackHistogramTest);
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           HoldbackDecisionTest);
  permissions::PredictionRequestFeatures BuildPredictionRequestFeatures(
      permissions::PermissionRequest* request);
  void LookupResponseReceived(
      bool is_on_device,
      permissions::RequestType request_type,
      bool lookup_succesful,
      bool response_from_cache,
      const absl::optional<permissions::GeneratePredictionsResponse>& response);
  PredictionSource GetPredictionTypeToUse(
      permissions::RequestType request_type);

  void set_likelihood_override(PredictionGrantLikelihood mock_likelihood) {
    likelihood_override_for_testing_ = mock_likelihood;
  }

  void OnModelExecutionComplete(
      const absl::optional<permissions::GeneratePredictionsResponse>& result);

  bool ShouldHoldBack(bool is_on_device, permissions::RequestType request_type);

  raw_ptr<Profile> profile_;
  std::unique_ptr<PredictionServiceRequest> request_;
  absl::optional<PredictionGrantLikelihood> last_request_grant_likelihood_;
  absl::optional<bool> was_decision_held_back_;

  absl::optional<PredictionGrantLikelihood> likelihood_override_for_testing_;

  DecisionMadeCallback callback_;

  base::WeakPtrFactory<PredictionBasedPermissionUiSelector> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
