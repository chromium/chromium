// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/permissions_ai.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
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
  // Contains information that are not important as features for the
  // prediction service, but contain details about the workflow and the origin
  // of feature data.
  struct PredictionRequestMetadata {
    permissions::PermissionPredictionSource prediction_source;
    permissions::RequestType request_type;
  };

  // Contains input data and metadata that are important for the
  // superset of model execution workflows supported by the ui selector.
  struct ModelExecutionData {
    permissions::PredictionRequestFeatures features;
    PredictionRequestMetadata request_metadata;
    permissions::PredictionModelType model_type;
    std::optional<std::string> inner_text;
    std::optional<SkBitmap>(snapshot);
    std::optional<passage_embeddings::Embedding> inner_text_embedding;

    ModelExecutionData(permissions::PredictionRequestFeatures features,
                       PredictionRequestMetadata request_metadata,
                       permissions::PredictionModelType model_type);
    ModelExecutionData();
    ~ModelExecutionData();
    ModelExecutionData(const ModelExecutionData&) = delete;
    ModelExecutionData(ModelExecutionData&&);
    ModelExecutionData& operator=(const ModelExecutionData&) = delete;
  };

  using PredictionGrantLikelihood =
      permissions::PermissionUiSelector::PredictionGrantLikelihood;

  using ModelExecutionCallback =
      base::OnceCallback<void(ModelExecutionData model_data)>;

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

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  void set_snapshot_for_testing(SkBitmap snapshot);
#endif

  void set_inner_text_for_testing(
      content_extraction::InnerTextResult inner_text);

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
      permissions::PermissionRequest* request,
      permissions::PermissionPredictionSource prediction_source);
  void LookupResponseReceived(
      base::TimeTicks model_inquire_start_time,
      PredictionRequestMetadata request_metadata,
      bool lookup_successful,
      bool response_from_cache,
      const std::optional<permissions::GeneratePredictionsResponse>& response);
  permissions::PermissionPredictionSource GetPredictionTypeToUse(
      permissions::RequestType request_type);

  void set_likelihood_override(PredictionGrantLikelihood mock_likelihood) {
    likelihood_override_for_testing_ = mock_likelihood;
  }

  // Part of the AIvX model execution chain; provided as a curryed callback to
  // be submitted to the logic that fetches the current page content text for
  // the AIvX model. The first two parameters are set by the callee; content of
  // model_data is used to call the on device model and by the server side model
  // later. When the text is fetched asynchronously with success,
  // model_execution_callback will get called to continue the chain of
  // asynchronously added input data.
  void OnGetInnerTextForOnDeviceModel(
      ModelExecutionData model_data,
      ModelExecutionCallback model_execution_callback,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  bool ShouldHoldBack(const PredictionRequestMetadata& request_metadata) const;

  void InquireServerModel(
      const permissions::PredictionRequestFeatures& features,
      PredictionRequestMetadata request_metadata);

  // As the first part of the AIv1 model execution chain, this function triggers
  // AIv1 input collection and model execution, with its output being input of
  // the follow-up CPSSv3 server side model execution. If the AIv1 model is not
  // available or is executed with an error, only the server side model will get
  // called.
  void InquireOnDeviceAiv1AndServerModelIfAvailable(
      content::RenderFrameHost* render_frame_host,
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata);

  // Function that handles model execution for all AIvX models.
  void ExecuteOnDeviceAivXModel(ModelExecutionData model_data);
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

  // As the first part of the AIv4 model execution chain, this function triggers
  // AIv4 input collection and model execution, with its output being input of
  // the follow-up CPSSv3 server side model execution. If the AIv4 model is not
  // available or is executed with an error, only the server side model will get
  // called.
  void InquireOnDeviceAiv4AndServerModelIfAvailable(
      content::WebContents* web_contents,
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata);

  // Part of the AIvX model execution chain; provided as a curryed callback to
  // be submitted to the logic that fetches a snapshot that serves as the input
  // for the AIvX models. The first three parameters are set by the callee, to
  // be used by the server side model later and for logging.
  void OnSnapshotTakenForOnDeviceModel(
      base::TimeTicks snapshot_inquire_start_time,
      ModelExecutionData model_data,
      const SkBitmap& screenshot);

  // Callback for tflite based AivX model handlers.
  void OnDeviceTfliteAivXModelExecutionCallback(
      base::TimeTicks model_inquire_start_time,
      permissions::PredictionRequestFeatures features,
      PredictionRequestMetadata request_metadata,
      permissions::PredictionModelType model_type,
      const std::optional<permissions::PermissionRequestRelevance>& relevance);

  // Use on device CPSSv1 tflite model.
  void InquireCpssV1OnDeviceModelIfAvailable(
      const permissions::PredictionRequestFeatures& features,
      PredictionRequestMetadata request_metadata);

  // Part of the AivX model workflow. Creates a snapshot asynchronously and
  // calls ExecuteOnDeviceAivXModel if the snapshot is not empty. If snapshot
  // creation failed, on-device model execution is not attempted and instead it
  // proceeds with the basic CPSSv3 workflow without the output of the
  // on-device model.
  void TakeSnapshot(content::RenderWidgetHostView* host_view,
                    ModelExecutionData model_data);

  // Extracts inner text asynchronously and runs the provided model execution
  // callback, which is meant to be a wrapper around ExecuteOnDeviceAivXModel.
  // Part of the AivX model workflow.
  void GetInnerText(content::RenderFrameHost* render_frame_host,
                    ModelExecutionData model_data,
                    ModelExecutionCallback model_execution_callback);

  // Part of Aiv4 workflow; to use the inner text as input to the tflite model,
  // we need to preprocess it with the passage embeddings model. If
  // rendered_text is an empty string, on-device model execution is not
  // attempted and instead it proceeds with the basic CPSSv3 workflow without
  // the output of the on-device model.
  void CreatePassageEmbeddingFromRenderedText(
      std::string rendered_text,
      passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback callback);

  // Callback for the passage embeddings model. Sets the
  // |passage_embeddings_task_id_| if the passage_embedder model is available.
  // Still running embedder tasks will get canceled upon calling this function.
  // Fills in the inner_text_embeddings field of the model_metadata on success
  // and calls the model_execution_callback in any case. Failures will get
  // propagated and should be handled by the model_execution_callback callback.
  void OnPassageEmbeddingsComputed(
      ModelExecutionData model_data,
      ModelExecutionCallback model_execution_callback,
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      passage_embeddings::Embedder::TaskId task_id,
      passage_embeddings::ComputeEmbeddingsStatus status);
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

  std::optional<content_extraction::InnerTextResult> inner_text_for_testing_;
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::optional<SkBitmap> snapshot_for_testing_;

  // Used to cancel a still running embedding task for the previous stale query
  // to the passage embedder model that we use to prepare the text input for
  // AIv4.
  std::optional<passage_embeddings::Embedder::TaskId>
      passage_embeddings_task_id_;
#endif

  // Used to asynchronously call the callback during on device model execution.
  base::WeakPtrFactory<PredictionBasedPermissionUiSelector> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_BASED_PERMISSION_UI_SELECTOR_H_
