// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/permissions_ai.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request_enums.h"
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
    USE_ONDEVICE_AI_AND_SERVER_SIDE,  // url based cpss v2 with on device AI
                                      // prediction
    USE_SERVER_SIDE,                  // url based cpss v2
    USE_ONDEVICE_TFLITE,              // on device cpss v1
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
  void SelectUiToUse(content::WebContents* web_contents,
                     permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  void Cancel() override;

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override;

  std::optional<PredictionGrantLikelihood> PredictedGrantLikelihoodForUKM()
      override;

  std::optional<permissions::PermissionRequestRelevance>
  PermissionRequestRelevanceForUKM() override;

  std::optional<bool> WasSelectorDecisionHeldback() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           GetPredictionTypeToUse);
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           GetPredictionTypeToUseTFLite);
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           HoldbackHistogramTest);
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           HoldbackDecisionTest);

  void AiOnDeviceModelExecutionCallback(
      permissions::PredictionRequestFeatures features,
      permissions::RequestType request_type,
      std::optional<optimization_guide::proto::PermissionsAiResponse> response);

  permissions::PredictionRequestFeatures BuildPredictionRequestFeatures(
      permissions::PermissionRequest* request);
  void LookupResponseReceived(
      base::TimeTicks model_inquire_start_time,
      bool is_on_device,
      permissions::RequestType request_type,
      bool lookup_successful,
      bool response_from_cache,
      const std::optional<permissions::GeneratePredictionsResponse>& response);
  PredictionSource GetPredictionTypeToUse(
      permissions::RequestType request_type);

  void set_likelihood_override(PredictionGrantLikelihood mock_likelihood) {
    likelihood_override_for_testing_ = mock_likelihood;
  }

  void OnGetInnerTextForOnDeviceModel(
      permissions::PredictionRequestFeatures features,
      permissions::RequestType request_type,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  void OnModelExecutionComplete(
      const std::optional<permissions::GeneratePredictionsResponse>& result);

  bool ShouldHoldBack(bool is_on_device, permissions::RequestType request_type);

  void InquireServerModel(
      const permissions::PredictionRequestFeatures& features,
      permissions::RequestType request_type,
      bool record_source);
  void InquireTfliteOnDeviceModelIfAvailable(
      const permissions::PredictionRequestFeatures& features,
      permissions::RequestType request_type);
  void InquireAiOnDeviceAndServerModelIfAvailable(
      content::RenderFrameHost* rfh,
      permissions::PredictionRequestFeatures features,
      permissions::RequestType request_type);

  raw_ptr<Profile> profile_;
  std::unique_ptr<PredictionServiceRequest> request_;
  std::optional<PredictionGrantLikelihood> last_request_grant_likelihood_;
  std::optional<permissions::PermissionRequestRelevance>
      last_permission_request_relevance_;
  std::optional<float> tflite_model_holdback_probability_;
  std::optional<bool> was_decision_held_back_;

  std::optional<PredictionGrantLikelihood> likelihood_override_for_testing_;

  DecisionMadeCallback callback_;

  // Used to asynchronously call the callback during on device model execution.
  base::WeakPtrFactory<PredictionBasedPermissionUiSelector> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
