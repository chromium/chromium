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
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/permissions_ai.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/request_type.h"
// pref_names include is needed for the browser tests to work as they cannot
// include this dependency themselves
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/render_widget_host_view.h"

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
    kNoCpssModel,
    kServerSideCpssV3Model,
    kOnDeviceCpssV1Model,
    kOnDeviceAiv1AndServerSideModel,
    kOnDeviceAiv3AndServerSideModel,
  };

  // Contains information that are not important as features for the
  // prediction service, but contain details about the workflow and the origin
  // of feature data.
  struct PredictionRequestMetadata {
    PredictionSource prediction_source;
    permissions::RequestType request_type;
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

  std::optional<permissions::PermissionRequestRelevance>
  get_permission_request_relevance_for_testing();

  void set_snapshot_for_testing(SkBitmap snapshot);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      PredictionBasedPermissionUiExpectedPredictionSourceTest,
      GetPredictionTypeToUse);
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           GetPredictionTypeToUseCpssV1);
  FRIEND_TEST_ALL_PREFIXES(
      PredictionBasedPermissionUiExpectedHoldbackChanceTest,
      HoldbackHistogramTest);
  FRIEND_TEST_ALL_PREFIXES(PredictionBasedPermissionUiSelectorTest,
                           HoldbackDecisionTest);

  // Callback for the Aiv1ModelHandler, with the first to parameters being
  // curryed to be used for the server side model call.
  void OnDeviceAiv1ModelExecutionCallback(
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata,
      std::optional<optimization_guide::proto::PermissionsAiResponse> response);

  permissions::PredictionRequestFeatures BuildPredictionRequestFeatures(
      permissions::PermissionRequest* request);
  void LookupResponseReceived(
      base::TimeTicks model_inquire_start_time,
      PredictionRequestMetadata request_metadata,
      bool lookup_successful,
      bool response_from_cache,
      const std::optional<permissions::GeneratePredictionsResponse>& response);
  PredictionSource GetPredictionTypeToUse(
      permissions::RequestType request_type);

  void set_likelihood_override(PredictionGrantLikelihood mock_likelihood) {
    likelihood_override_for_testing_ = mock_likelihood;
  }

  // Part of the AIv1 model execution chain; provided as a curryed callback to
  // be submitted to the logic that fetches the current page content text for
  // the AIv1 model. The first two parameters are set by the callee, to be used
  // by the server side model later.
  void OnGetInnerTextForOnDeviceModel(
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  bool ShouldHoldBack(const PredictionRequestMetadata& request_metadata) const;

  void InquireServerModel(
      const permissions::PredictionRequestFeatures& features,
      PredictionRequestMetadata request_metadata,
      bool record_source);

  // As the first part of the AIv3 model execution chain, this function triggers
  // AIv3 input collection and model execution, with its output being input of
  // the follow-up CPSSv3 server side model execution. If the AIv3 model is not
  // available or is executed with an error, only the server side model will get
  // called.
  void InquireOnDeviceAiv1AndServerModelIfAvailable(
      content::RenderFrameHost* rfh,
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // As the first part of the AIv3 model execution chain, this function triggers
  // AIv3 input collection and model execution, with its output being input of
  // the follow-up CPSSv3 server side model execution. If the AIv3 model is not
  // available or is executed with an error, only the server side model will get
  // called.
  void InquireOnDeviceAiv3AndServerModelIfAvailable(
      content::RenderWidgetHostView* host_view,
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata);

  // Part of the AIv3 model execution chain; provided as a curryed callback to
  // be submitted to the logic that fetches a snapshot that serves as the input
  // for the AIv3 model. The first two parameters are set by the callee, to be
  // used by the server side model later.
  void OnSnapshotTakenForOnDeviceModel(
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata,
      const SkBitmap& screenshot);

  // Callback for the Aiv3ModelHandler, with the first to parameters being
  // curryed to be used for the server side model call.
  void OnDeviceAiv3ModelExecutionCallback(
      base::TimeTicks model_inquire_start_time,
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata,
      const std::optional<permissions::PermissionRequestRelevance>& relevance);

  // Use on device CPSSv1 tflite model.
  void InquireCpssV1OnDeviceModelIfAvailable(
      const permissions::PredictionRequestFeatures& features,
      PredictionRequestMetadata request_metadata);
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  raw_ptr<Profile> profile_;
  std::unique_ptr<PredictionServiceRequest> request_;
  std::optional<PredictionGrantLikelihood> last_request_grant_likelihood_;
  std::optional<permissions::PermissionRequestRelevance>
      last_permission_request_relevance_;
  std::optional<float> cpss_v1_model_holdback_probability_;
  std::optional<bool> was_decision_held_back_;

  std::optional<PredictionGrantLikelihood> likelihood_override_for_testing_;

  DecisionMadeCallback callback_;

  std::optional<SkBitmap> snapshot_for_testing_;

  // Used to asynchronously call the callback during on device model execution.
  base::WeakPtrFactory<PredictionBasedPermissionUiSelector> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
