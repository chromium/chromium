// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/permissions/permission_actions_history.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/prediction_service/prediction_request_features.h"

class PredictionServiceRequest;
class Profile;

namespace permissions {
struct PredictionRequestFeatures;
class GeneratePredictionsResponse;
}  // namespace permissions

// Each instance of this class is long-lived and can support multiple requests,
// but only one at a time.
class PredictionBasedPermissionUiSelector
    : public permissions::NotificationPermissionUiSelector {
 public:
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

  base::Optional<PredictionGrantLikelihood> PredictedGrantLikelihoodForUKM()
      override;

 private:
  permissions::PredictionRequestFeatures BuildPredictionRequestFeatures(
      permissions::PermissionRequest* request);
  void LookupReponseReceived(
      bool lookup_succesful,
      bool response_from_cache,
      std::unique_ptr<permissions::GeneratePredictionsResponse> response);
  bool IsAllowedToUseAssistedPrompts();
  static void FillInActionCounts(
      permissions::PredictionRequestFeatures::ActionCounts* counts,
      const std::vector<PermissionActionsHistory::Entry>& permission_actions);

  void set_likelihood_override(PredictionGrantLikelihood mock_likelihood) {
    likelihood_override_for_testing_ = mock_likelihood;
  }

  Profile* profile_;
  std::unique_ptr<PredictionServiceRequest> request_;
  base::Optional<PredictionGrantLikelihood> last_request_grant_likelihood_;

  base::Optional<PredictionGrantLikelihood> likelihood_override_for_testing_;

  DecisionMadeCallback callback_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
