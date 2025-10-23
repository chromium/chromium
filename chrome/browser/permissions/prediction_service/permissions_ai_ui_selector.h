// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_UI_SELECTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/request_type.h"
// pref_names include is needed for the browser tests to work as they cannot
// include this dependency themselves
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/render_widget_host_view.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/permissions/prediction_service/language_detection_observer.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#endif

class PredictionServiceRequest;
class Profile;

namespace permissions {
struct PredictionRequestFeatures;
class GeneratePredictionsResponse;
class PassageEmbedderDelegate;
}  // namespace permissions

// Each instance of this class is long-lived and can support multiple requests,
// but only one at a time.
class PermissionsAiUiSelector : public permissions::PermissionUiSelector {
 public:
  // Contains information that are not important as features for the
  // prediction service, but contain details about the workflow and the origin
  // of feature data.
  struct PredictionRequestMetadata {
    permissions::PermissionPredictionSource prediction_source;
    permissions::RequestType request_type;
  };

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Contains input data and metadata that are important for the
  // superset of model execution workflows supported by the ui selector.
  struct ModelExecutionData {
    permissions::PredictionRequestFeatures features;
    PredictionRequestMetadata request_metadata;
    permissions::PredictionModelType model_type;
    std::optional<std::string> inner_text;
    std::optional<SkBitmap> snapshot;
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

  using ModelExecutionCallback =
      base::OnceCallback<void(ModelExecutionData model_data)>;
#endif

  using PredictionGrantLikelihood =
      permissions::PermissionUiSelector::PredictionGrantLikelihood;

  // The timeout to ensure that deciding which UI to chose for a permission
  // request is not taking longer than n seconds..
  static const int kPermissionRequestUiDecisionTimeout = 3;

  // Constructs an instance in the context of the given |profile|.
  explicit PermissionsAiUiSelector(Profile* profile);
  ~PermissionsAiUiSelector() override;

  PermissionsAiUiSelector(const PermissionsAiUiSelector&) = delete;
  PermissionsAiUiSelector& operator=(const PermissionsAiUiSelector&) = delete;

  // NotificationPermissionUiSelector:
  void SelectUiToUse(content::WebContents* web_contents,
                     permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  // Resets the callback, the language detection observer and the passage
  // embedder delegate, which cancels all async operations managed by them.
  void Cancel() override;

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override;

  std::optional<PredictionGrantLikelihood> PredictedGrantLikelihoodForUKM()
      override;

  std::optional<permissions::PermissionRequestRelevance>
  PermissionRequestRelevanceForUKM() override;

  std::optional<permissions::PermissionAiRelevanceModel>
  PermissionAiRelevanceModelForUKM() override;

  std::optional<bool> WasSelectorDecisionHeldback() override;

  std::optional<permissions::PermissionRequestRelevance>
  get_permission_request_relevance_for_testing();

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  void set_snapshot_for_testing(SkBitmap snapshot);
#endif

  void set_inner_text_for_testing(
      content_extraction::InnerTextResult inner_text);

  void set_language_detection_observer_for_testing(
      std::unique_ptr<permissions::LanguageDetectionObserver>
          language_detection_observer);

  void set_callback_for_testing(DecisionMadeCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(
      PredictionBasedPermissionUiExpectedPredictionSourceTest,
      GetPredictionTypeToUse);
  FRIEND_TEST_ALL_PREFIXES(PermissionsAiUiSelectorTest,
                           GetPredictionTypeToUseCpssV1);
  FRIEND_TEST_ALL_PREFIXES(
      PredictionBasedPermissionUiExpectedHoldbackChanceTest,
      HoldbackHistogramTest);
  FRIEND_TEST_ALL_PREFIXES(PermissionsAiUiSelectorTest, HoldbackDecisionTest);

  // A safe way to invoke the callback with a decision.
  void FinishRequest(Decision decision, bool timeout = false);

  // Resets the permission request, the language detection observer and the
  // passage embedder delegate, which cancels all async operations managed by
  // them.
  void Cleanup();

  // Called when the global timeout for the entire UI selection process is
  // reached.
  void OnTimeout();

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

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Function that handles model execution for all AIvX models.
  void ExecuteOnDeviceAivXModel(ModelExecutionData model_data);
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

  // Part of the AivX model workflow.
  // Extracts inner text asynchronously and runs the provided model execution
  // callback, which is meant to be a wrapper around ExecuteOnDeviceAivXModel.
  // If rendered_text is an empty string, on-device model execution is not
  // attempted and instead it proceeds with the basic CPSSv3 workflow without
  // the output of the on-device model.
  void GetInnerText(content::RenderFrameHost* render_frame_host,
                    ModelExecutionData model_data,
                    ModelExecutionCallback model_execution_callback);

  // Callback for the passage embeddings delegate. Will get called if embeddings
  // were computed with success. (Otherwise, server side CPSSv3 model will get
  // inquired instead of Aiv4.)
  void OnPassageEmbeddingsComputed(
      ModelExecutionData model_data,
      ModelExecutionCallback model_execution_callback,
      passage_embeddings::Embedding embedding);
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  raw_ptr<Profile> profile_;
  std::unique_ptr<PredictionServiceRequest> request_;
  std::optional<PredictionGrantLikelihood> last_request_grant_likelihood_;
  std::optional<permissions::PermissionRequestRelevance>
      last_permission_request_relevance_;
  std::optional<permissions::PermissionAiRelevanceModel>
      last_permission_ai_relevance_model_;
  std::optional<float> cpss_v1_model_holdback_probability_;
  std::optional<bool> was_decision_held_back_;

  std::optional<PredictionGrantLikelihood> likelihood_override_for_testing_;

  DecisionMadeCallback callback_;

  base::OneShotTimer timeout_timer_;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::optional<content_extraction::InnerTextResult> inner_text_for_testing_;
  std::optional<SkBitmap> snapshot_for_testing_;

  //  Handles calls to the passage embedder, a tflite model that is used to
  //  compute the embeddings used as input for the AIv4 model.
  //  Needs to be initialized after profile_;
  std::unique_ptr<permissions::PassageEmbedderDelegate>
      passage_embedder_delegate_;

  // For the Aiv4 execution flow we use a text embeddings model that only works
  // for the English language. Therefore we use an observer to wait for language
  // detection to finish (in case its not already done when we need this).
  //  Needs to be initialized after profile_;
  std::unique_ptr<permissions::LanguageDetectionObserver>
      language_detection_observer_;

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Used to asynchronously call the callback during on device model execution.
  base::WeakPtrFactory<PermissionsAiUiSelector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_UI_SELECTOR_H_
