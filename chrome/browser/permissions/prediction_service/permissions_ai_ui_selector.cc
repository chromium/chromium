// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/permissions_ai_ui_selector.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/default_clock.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/prediction_service/passage_embedder_delegate.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider_factory.h"
#include "chrome/browser/permissions/prediction_service/prediction_service_factory.h"
#include "chrome/browser/permissions/prediction_service/prediction_service_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_service.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/unified_consent/pref_names.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

// TODO(crbug.com/382447738): Fix tflite defines; this might not build for
// tflite right now.
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/prediction_service/permissions_aiv4_handler.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace {
using ComputePassagesEmbeddingsCallback =
    ::passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback;
using ::permissions::LanguageDetectionObserver;
using ::permissions::PassageEmbedderDelegate;
using ::permissions::PermissionRequest;
using ::permissions::PermissionRequestRelevance;
using ::permissions::PermissionsAiv3Handler;
using ::permissions::PermissionsAiv4Handler;
using ::permissions::PermissionUiSelector;
using ::permissions::PermissionUmaUtil;
using ::permissions::PredictionModelHandlerProvider;
using ::permissions::PredictionModelType;
using ::permissions::PredictionRequestFeatures;
using QuietUiReason = PermissionsAiUiSelector::QuietUiReason;
using Decision = PermissionsAiUiSelector::Decision;
using PredictionSource = ::permissions::PermissionPredictionSource;

constexpr auto VeryUnlikely = permissions::
    PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;
constexpr auto Unlikely =
    permissions::PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY;

// The data we consider can only be at most 28 days old to match the data that
// the ML model is built on.
constexpr base::TimeDelta kPermissionActionCutoffAge = base::Days(28);

// Only send requests if there are at least 4 action in the user's history for
// the particular permission type.
constexpr size_t kRequestedPermissionMinimumHistoricalActions = 4;

// The maximum length of a page's content. For now, page content is limited by
// the passage embedders 64 token limit. We therefore limit the input text as
// well.
constexpr size_t kPageContentMaxLength = 500;

// The minimum length of a page's content. It is needed to avoid analyzing pages
// with too short text.
constexpr size_t kPageContentMinLength = 10;

std::optional<
    permissions::PermissionPrediction_Likelihood_DiscretizedLikelihood>
ParsePredictionServiceMockLikelihood(const std::string& value) {
  if (value == "very-unlikely") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;
  } else if (value == "unlikely") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY;
  } else if (value == "neutral") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_NEUTRAL;
  } else if (value == "likely") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_LIKELY;
  } else if (value == "very-likely") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_LIKELY;
  }

  return std::nullopt;
}

bool ShouldPredictionTriggerQuietUi(
    PermissionUiSelector::PredictionGrantLikelihood likelihood) {
  if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIP92)) {
    return likelihood == Unlikely || likelihood == VeryUnlikely;
  }
  return likelihood == VeryUnlikely;
}

PermissionUiSelector::GeolocationAccuracy GetPredictedGeolocationAccuracy(
    const permissions::GeneratePredictionsResponse& response) {
  if (!response.prediction(0).has_geolocation_prediction()) {
    return PermissionUiSelector::GeolocationAccuracy::kUnspecified;
  }
  switch (response.prediction(0).geolocation_prediction().accuracy()) {
    case permissions::PermissionPrediction::GeolocationPrediction::
        ACCURACY_UNSPECIFIED:
      return PermissionUiSelector::GeolocationAccuracy::kUnspecified;
    case permissions::PermissionPrediction::GeolocationPrediction::
        ACCURACY_PRECISE:
      return PermissionUiSelector::GeolocationAccuracy::kPrecise;
    case permissions::PermissionPrediction::GeolocationPrediction::
        ACCURACY_APPROXIMATE:
      return PermissionUiSelector::GeolocationAccuracy::kApproximate;
  };
}

}  // namespace

inline PermissionsAiUiSelector::ModelExecutionData::ModelExecutionData() =
    default;
inline PermissionsAiUiSelector::ModelExecutionData::ModelExecutionData(
    PermissionsAiUiSelector::ModelExecutionData&&) = default;
inline PermissionsAiUiSelector::ModelExecutionData::~ModelExecutionData() =
    default;

PermissionsAiUiSelector::ModelExecutionData::ModelExecutionData(
    permissions::PredictionRequestFeatures features,
    PredictionRequestMetadata request_metadata,
    permissions::PredictionModelType model_type)
    : features(std::move(features)),
      request_metadata(std::move(request_metadata)),
      model_type(model_type) {}

PermissionsAiUiSelector::PermissionsAiUiSelector(Profile* profile)
    : profile_(profile)
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
      ,
      passage_embedder_delegate_(
          std::make_unique<PassageEmbedderDelegate>(profile_)),
      language_detection_observer_(
          std::make_unique<LanguageDetectionObserver>())
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
{
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPredictionServiceMockLikelihood)) {
    auto mock_likelihood = ParsePredictionServiceMockLikelihood(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kPredictionServiceMockLikelihood));
    if (mock_likelihood.has_value()) {
      set_likelihood_override(mock_likelihood.value());
    }
  }
}

PermissionsAiUiSelector::~PermissionsAiUiSelector() = default;

void PermissionsAiUiSelector::InquireServerModel(
    const PredictionRequestFeatures& features,
    PredictionRequestMetadata request_metadata) {
  permissions::PredictionService* service =
      PredictionServiceFactory::GetForProfile(profile_);

  VLOG(1) << "[CPSS] Starting prediction service request";

  request_ = std::make_unique<PredictionServiceRequest>(
      service, features,
      base::BindOnce(&PermissionsAiUiSelector::LookupResponseReceived,
                     base::Unretained(this),
                     /*model_inquire_start_time=*/base::TimeTicks::Now(),
                     std::move(request_metadata)));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PermissionsAiUiSelector::InquireCpssV1OnDeviceModelIfAvailable(
    const PredictionRequestFeatures& features,
    PredictionRequestMetadata request_metadata) {
  PredictionModelHandlerProvider* prediction_model_handler_provider =
      PredictionModelHandlerProviderFactory::GetForBrowserContext(profile_);
  permissions::PredictionModelHandler* prediction_model_handler = nullptr;
  if (prediction_model_handler_provider) {
    prediction_model_handler =
        prediction_model_handler_provider->GetPredictionModelHandler(
            request_metadata.request_type);
  }
  if (prediction_model_handler && prediction_model_handler->ModelAvailable() &&
      prediction_model_handler->GetModelInfo().has_value()) {
    VLOG(1) << "[CPSS] Using locally available CPSSv1 model";
    auto proto_request = GetPredictionRequestProto(features);
    cpss_v1_model_holdback_probability_ =
        prediction_model_handler->HoldBackProbability();
    prediction_model_handler->ExecuteModelWithMetadata(
        base::BindOnce(&PermissionsAiUiSelector::LookupResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*model_inquire_start_time=*/base::TimeTicks::Now(),
                       std::move(request_metadata),
                       /*lookup_succesful=*/true,
                       /*response_from_cache=*/false),
        std::move(proto_request));
    return;
  }
  VLOG(1) << "[CPSS] On device CPSSv1 model unavailable";
  FinishRequest(Decision::UseNormalUiAndShowNoWarning());
}

void PermissionsAiUiSelector::InquireOnDeviceAiv3AndServerModelIfAvailable(
    content::RenderWidgetHostView* host_view,
    permissions::PredictionRequestFeatures features,
    PredictionRequestMetadata request_metadata) {
  last_permission_ai_relevance_model_ =
      permissions::PermissionAiRelevanceModel::kAIv3;
  VLOG(1) << "[PermissionsAIv3] On device AI prediction requested";
  TakeSnapshot(host_view, {std::move(features), std::move(request_metadata),
                           PredictionModelType::kOnDeviceAiV3Model});
}

void PermissionsAiUiSelector::InquireOnDeviceAiv4AndServerModelIfAvailable(
    content::WebContents* web_contents,
    permissions::PredictionRequestFeatures features,
    PredictionRequestMetadata request_metadata) {
  VLOG(1) << "[PermissionsAIv4] On device AI prediction requested";

  last_permission_ai_relevance_model_ =
      permissions::PermissionAiRelevanceModel::kAIv4;

  auto language_detected_cbk = base::BindOnce(
      &PermissionsAiUiSelector::GetInnerText, weak_ptr_factory_.GetWeakPtr(),
      web_contents->GetPrimaryMainFrame(),
      ModelExecutionData{features, request_metadata,
                         PredictionModelType::kOnDeviceAiV4Model},
      base::BindOnce(&PermissionsAiUiSelector::TakeSnapshot,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetRenderWidgetHostView()));

  language_detection_observer_->Init(
      web_contents, std::move(language_detected_cbk),
      /*on_fallback=*/
      base::BindOnce(&PermissionsAiUiSelector::InquireServerModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(features),
                     std::move(request_metadata)));
}

void PermissionsAiUiSelector::OnSnapshotTakenForOnDeviceModel(
    base::TimeTicks snapshot_inquire_start_time,
    ModelExecutionData model_data,
    const SkBitmap& snapshot) {
  VLOG(1) << "[PermissionsAI] OnSnapshotTakenForOnDeviceModel";
  PermissionUmaUtil::RecordSnapshotTakenTimeAndSuccessForAivX(
      model_data.model_type, snapshot_inquire_start_time,
      /*success=*/!snapshot.drawsNothing());
  if (snapshot.drawsNothing()) {
    VLOG(1) << "[PermissionsAI] The page's snapshot is empty; skipping AivX "
               "on-device model execution.";
    return InquireServerModel(model_data.features,
                              std::move(model_data.request_metadata));
  }
  model_data.snapshot = std::move(snapshot);
  ExecuteOnDeviceAivXModel(std::move(model_data));
}

void PermissionsAiUiSelector::OnDeviceTfliteAivXModelExecutionCallback(
    base::TimeTicks model_inquire_start_time,
    permissions::PredictionRequestFeatures features,
    PredictionRequestMetadata request_metadata,
    permissions::PredictionModelType model_type,
    const std::optional<PermissionRequestRelevance>& relevance) {
  PermissionUmaUtil::RecordPredictionModelInquireTime(model_type,
                                                      model_inquire_start_time);
  VLOG(1) << "[PermissionsAI]: Model execution callback called "
          << (relevance.has_value() ? "with value" : "without value");
  if (relevance.has_value()) {
    VLOG(1) << "[PermissionsAI]: PermissionRequest has a relevance of "
            << static_cast<int>(relevance.value());
    last_permission_request_relevance_ = relevance.value();
  } else {
    last_permission_request_relevance_ =
        PermissionRequestRelevance::kUnspecified;
  }

  features.permission_relevance = last_permission_request_relevance_.value();

  PermissionUmaUtil::RecordPermissionRequestRelevance(
      request_metadata.request_type, features.permission_relevance, model_type);

  InquireServerModel(features, std::move(request_metadata));
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

void PermissionsAiUiSelector::SelectUiToUse(
    content::WebContents* web_contents,
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  VLOG(1) << "[CPSS] Selector activated";

  // If callback is already set, it means that the selector was already
  // activated for a previous permission request and the decision has not been
  // delivered yet. This can happen if the page triggers a permission request
  // while the previous permission request is still pending. In this case, we
  // ignore the new request as we cannot stop previously activated evaluation.
  // callback_ is reset to prevent the decision from being delivered to the
  // obsolete request.
  if (callback_) {
    VLOG(1) << "[CPSS] Concurrent permission requests evaluations are not "
               "supported.";
    Cancel();

    PermissionUmaUtil::RecordPermissionPredictionConcurrentRequests(
        request->request_type());

    std::move(callback).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  callback_ = std::move(callback);
  timeout_timer_.Start(FROM_HERE,
                       base::Seconds(kPermissionRequestUiDecisionTimeout),
                       base::BindOnce(&PermissionsAiUiSelector::OnTimeout,
                                      weak_ptr_factory_.GetWeakPtr()));
  last_permission_ai_relevance_model_ = std::nullopt;
  last_permission_request_relevance_ = std::nullopt;
  last_request_grant_likelihood_ = std::nullopt;
  cpss_v1_model_holdback_probability_ = std::nullopt;
  was_decision_held_back_ = std::nullopt;
  language_detection_observer_->Reset();

  bool is_tflite_available = true;
  // BUILD_WITH_TFLITE_LIB should be enabled for most of the devices on all
  // platforms. However, it is still useful to measure the percentage of
  // disabled devices.
#if !BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  is_tflite_available = false;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  base::UmaHistogramBoolean("Permissions.PredictionService.TFLiteLibAvailable",
                            is_tflite_available);

  const PredictionSource prediction_source =
      GetPredictionTypeToUse(request->request_type());

  PermissionUmaUtil::RecordPermissionPredictionSource(prediction_source,
                                                      request->request_type());

  if (prediction_source == PredictionSource::kNoCpssModel) {
    VLOG(1) << "[CPSS] Configuration does not allow CPSS requests";
    FinishRequest(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  PredictionRequestFeatures features =
      BuildPredictionRequestFeatures(request, prediction_source);
  if (prediction_source == PredictionSource::kOnDeviceCpssV1Model) {
    if (features.requested_permission_counts.total() <
        kRequestedPermissionMinimumHistoricalActions) {
      VLOG(1) << "[CPSS] Historic prompt count ("
              << features.requested_permission_counts.total()
              << ") is smaller than threshold ("
              << kRequestedPermissionMinimumHistoricalActions << ")";
      FinishRequest(Decision::UseNormalUiAndShowNoWarning());
      return;
    }
  }

  if (likelihood_override_for_testing_.has_value()) {
    VLOG(1) << "[CPSS] Using likelihood override value that was provided via "
               "command line";
    if (ShouldPredictionTriggerQuietUi(
            likelihood_override_for_testing_.value())) {
      FinishRequest(Decision::UseQuietUi(
          QuietUiReason::kServicePredictedVeryUnlikelyGrant,
          Decision::ShowNoWarning()));
    } else {
      FinishRequest(Decision::UseNormalUiAndShowNoWarning());
    }
    return;
  }

  DCHECK(!request_);
  PredictionRequestMetadata request_metadata = {
      .prediction_source = prediction_source,
      .request_type = request->request_type()};

  switch (prediction_source) {
    case PredictionSource::kServerSideCpssV3Model:
      return InquireServerModel(features, std::move(request_metadata));
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    case PredictionSource::kOnDeviceAiv4AndServerSideModel:
      return InquireOnDeviceAiv4AndServerModelIfAvailable(
          web_contents, std::move(features), std::move(request_metadata));
    case PredictionSource::kOnDeviceAiv3AndServerSideModel:
      return InquireOnDeviceAiv3AndServerModelIfAvailable(
          web_contents->GetRenderWidgetHostView(), std::move(features),
          std::move(request_metadata));
    case PredictionSource::kOnDeviceCpssV1Model:
      return InquireCpssV1OnDeviceModelIfAvailable(features,
                                                   std::move(request_metadata));
#else
    case PredictionSource::kOnDeviceAiv3AndServerSideModel:
      [[fallthrough]];
    case PredictionSource::kOnDeviceCpssV1Model:
      VLOG(1) << "[CPSS] Client doesn't support on-device tflite: "
              << static_cast<int>(prediction_source);
      FinishRequest(Decision::UseNormalUiAndShowNoWarning());
      return;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    case PredictionSource::kNoCpssModel:
      [[fallthrough]];
    default:
      NOTREACHED();
  }
}

void PermissionsAiUiSelector::OnGetInnerTextForOnDeviceModel(
    ModelExecutionData model_data,
    ModelExecutionCallback model_execution_callback,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  VLOG(1) << "[PermissionsAI] OnGetInnerTextForOnDeviceModel";
  if (result) {
    PermissionUmaUtil::RecordRenderedTextSize(
        model_data.model_type, model_data.request_metadata.request_type,
        result->inner_text.size());
  }

  bool rendered_text_useful =
      result && result->inner_text.size() > kPageContentMinLength;
  PermissionUmaUtil::RecordRenderedTextAcquireSuccessForAivX(
      model_data.model_type,
      /*success=*/rendered_text_useful);

  if (rendered_text_useful) {
    VLOG(1) << "[PermissionsAI] OnGetInnerTextForOnDeviceModel: "
               "rendered_text_useful true";
    std::string inner_text = std::move(result->inner_text);
    if (inner_text.size() > kPageContentMaxLength) {
      inner_text.resize(kPageContentMaxLength);
    }

    auto fallback_callback =
        base::BindOnce(&PermissionsAiUiSelector::InquireServerModel,
                       weak_ptr_factory_.GetWeakPtr(),
                       PredictionRequestFeatures(model_data.features),
                       model_data.request_metadata);

    auto on_passage_embeddings_computed_callback =
        base::BindOnce(&PermissionsAiUiSelector::OnPassageEmbeddingsComputed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(model_data),
                       std::move(model_execution_callback));

    return passage_embedder_delegate_->CreatePassageEmbeddingFromRenderedText(
        std::move(inner_text),
        std::move(on_passage_embeddings_computed_callback),
        std::move(fallback_callback));
  }

  VLOG(1) << "[PermissionsAI] The page's content is too short or empty; "
             "skipping execution of AivX on-device model";
  InquireServerModel(model_data.features,
                     std::move(model_data.request_metadata));
}

void PermissionsAiUiSelector::OnTimeout() {
  VLOG(1) << "[CPSS] Overall timeout for prediction reached.";
  Cleanup();
  FinishRequest(Decision::UseNormalUiAndShowNoWarning(), /*timeout=*/true);
}

void PermissionsAiUiSelector::Cancel() {
  timeout_timer_.Stop();
  callback_.Reset();
  Cleanup();
}

void PermissionsAiUiSelector::FinishRequest(Decision decision, bool timeout) {
  timeout_timer_.Stop();
  PermissionUmaUtil::RecordPredictionServiceTimeout(timeout);
  if (!callback_) {
    VLOG(1) << "[CPSS] FinishRequest called but callback is null";
    return;
  }

  VLOG(1) << "[CPSS] Finishing permission prediction request.";
  std::move(callback_).Run(std::move(decision));
}

void PermissionsAiUiSelector::Cleanup() {
  request_.reset();
  passage_embedder_delegate_->Reset();
  language_detection_observer_->Reset();
}

bool PermissionsAiUiSelector::IsPermissionRequestSupported(
    permissions::RequestType request_type) {
  return request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation;
}

std::optional<PermissionUiSelector::PredictionGrantLikelihood>
PermissionsAiUiSelector::PredictedGrantLikelihoodForUKM() {
  return last_request_grant_likelihood_;
}

std::optional<PermissionRequestRelevance>
PermissionsAiUiSelector::PermissionRequestRelevanceForUKM() {
  return last_permission_request_relevance_;
}

std::optional<permissions::PermissionAiRelevanceModel>
PermissionsAiUiSelector::PermissionAiRelevanceModelForUKM() {
  return last_permission_ai_relevance_model_;
}

std::optional<bool> PermissionsAiUiSelector::WasSelectorDecisionHeldback() {
  return was_decision_held_back_;
}

PredictionRequestFeatures
PermissionsAiUiSelector::BuildPredictionRequestFeatures(
    PermissionRequest* request,
    PredictionSource prediction_source) {
  PredictionRequestFeatures features;
  features.gesture = request->GetGestureType();
  features.type = request->request_type();
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionDedicatedCpssSettingAndroid)) {
    features.url = request->requesting_origin().GetWithEmptyPath();
  }
#else
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionPredictionsV2)) {
    features.url = request->requesting_origin().GetWithEmptyPath();
  }
#endif

  // Init `permission_relevance` here to avoid a crash during
  // `ConvertToProtoRelevance` execution.
  features.permission_relevance = PermissionRequestRelevance::kUnspecified;

  switch (prediction_source) {
    case PredictionSource::kOnDeviceAiv3AndServerSideModel:
      features.experiment_id =
          PredictionRequestFeatures::ExperimentId::kAiV3ExperimentId;
      break;
    case PredictionSource::kOnDeviceAiv4AndServerSideModel:
      features.experiment_id =
          PredictionRequestFeatures::ExperimentId::kAiV4ExperimentId;
      break;
    default:
      features.experiment_id =
          PredictionRequestFeatures::ExperimentId::kNoExperimentId;
  }

  base::Time cutoff = base::Time::Now() - kPermissionActionCutoffAge;

  permissions::PermissionActionsHistory* action_history =
      PermissionActionsHistoryFactory::GetForProfile(profile_);

  auto actions = action_history->GetHistory(
      cutoff, request->request_type(),
      permissions::PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);
  permissions::PermissionActionsHistory::FillInActionCounts(
      &features.requested_permission_counts, actions);

  actions = action_history->GetHistory(
      cutoff,
      permissions::PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);
  permissions::PermissionActionsHistory::FillInActionCounts(
      &features.all_permission_counts, actions);

  return features;
}

void PermissionsAiUiSelector::LookupResponseReceived(
    base::TimeTicks model_inquire_start_time,
    PredictionRequestMetadata request_metadata,
    bool lookup_successful,
    bool response_from_cache,
    const std::optional<permissions::GeneratePredictionsResponse>& response) {
  // This function is used as callback for request to the CPSSv1 on-device
  // model and the CPSSv3 server-side model. As we have multiple prediction
  // sources that use the server side model in the end, we check for the
  // CPSSv1 here and set is_on_device depending on this.
  bool is_on_device_cpss_v1 = request_metadata.prediction_source ==
                              PredictionSource::kOnDeviceCpssV1Model;
  PermissionUmaUtil::RecordPredictionModelInquireTime(
      is_on_device_cpss_v1 ? PredictionModelType::kOnDeviceCpssV1Model
                           : PredictionModelType::kServerSideCpssV3Model,
      model_inquire_start_time);

  request_.reset();
  if (!callback_) {
    VLOG(1) << "[CPSS] Prediction service response ignored as the request is "
               "canceled";
    return;
  }
  if (!lookup_successful || !response || response->prediction_size() == 0) {
    VLOG(1) << "[CPSS] Prediction service request failed because "
            << (!lookup_successful ? "the lookup was not successful."
                                   : (!response ? "the response is empty."
                                                : "the prediction is empty."));
    FinishRequest(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  last_request_grant_likelihood_ =
      response->prediction(0).grant_likelihood().discretized_likelihood();

  if (ShouldHoldBack(request_metadata)) {
    VLOG(1) << "[CPSS] Prediction service decision held back";
    was_decision_held_back_ = true;
    FinishRequest(Decision::UseNormalUi(
        Decision::ShowNoWarning(), GetPredictedGeolocationAccuracy(*response)));
    return;
  }
  was_decision_held_back_ = false;
  VLOG(1) << "[CPSS] Prediction service request succeeded and received "
             "likelihood: "
          << last_request_grant_likelihood_.value();

  if (ShouldPredictionTriggerQuietUi(last_request_grant_likelihood_.value())) {
    FinishRequest(Decision::UseQuietUi(
        is_on_device_cpss_v1
            ? QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant
            : QuietUiReason::kServicePredictedVeryUnlikelyGrant,
        Decision::ShowNoWarning()));
    return;
  }

  FinishRequest(Decision(Decision::UseNormalUi(
      Decision::ShowNoWarning(), GetPredictedGeolocationAccuracy(*response))));
}

bool PermissionsAiUiSelector::ShouldHoldBack(
    const PredictionRequestMetadata& request_metadata) const {
  permissions::RequestType request_type = request_metadata.request_type;
  PredictionSource prediction_source = request_metadata.prediction_source;
  DCHECK(request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation);

  // Holdback probability for this request.
  const double holdback_chance = base::RandDouble();
  bool should_holdback = false;
  PredictionModelType prediction_model;

  should_holdback =
      holdback_chance <
      permissions::feature_params::kPermissionPredictionsV2HoldbackChance.Get();

  switch (prediction_source) {
    case PredictionSource::kOnDeviceCpssV1Model:
      DCHECK(cpss_v1_model_holdback_probability_.has_value());
      should_holdback = holdback_chance < *cpss_v1_model_holdback_probability_;
      prediction_model = PredictionModelType::kOnDeviceCpssV1Model;
      break;
      // For on-device model + server-side model requests we will use the
      // holdback logic for the server-side model execution.
    case PredictionSource::kOnDeviceAiv4AndServerSideModel:
      prediction_model = PredictionModelType::kOnDeviceAiV4Model;
      break;
    case PredictionSource::kOnDeviceAiv3AndServerSideModel:
      prediction_model = PredictionModelType::kOnDeviceAiV3Model;
      break;
    case PredictionSource::kServerSideCpssV3Model:
      prediction_model = PredictionModelType::kServerSideCpssV3Model;
      break;
    default:
      NOTREACHED();
  }
  PermissionUmaUtil::RecordPermissionPredictionServiceHoldback(
      request_type, prediction_model, should_holdback);
  return should_holdback;
}

PredictionSource PermissionsAiUiSelector::GetPredictionTypeToUse(
    permissions::RequestType request_type) {
  const bool is_msbb_enabled = profile_->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);

  base::UmaHistogramBoolean("Permissions.PredictionService.MSBB",
                            is_msbb_enabled);

  VLOG(1) << "[CPSS] GetPredictionTypeToUse MSBB: " << is_msbb_enabled;

  const bool is_notification_cpss_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kEnableNotificationCPSS);

  VLOG(1) << "[CPSS] GetPredictionTypeToUse NotificationCPSS: "
          << is_notification_cpss_enabled;

  const bool is_geolocation_cpss_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kEnableGeolocationCPSS);

  VLOG(1) << "[CPSS] GetPredictionTypeToUse GeolocationCPSS: "
          << is_geolocation_cpss_enabled;

  if (request_type == permissions::RequestType::kNotifications &&
      !is_notification_cpss_enabled) {
    VLOG(1) << "[CPSS] GetPredictionTypeToUse NoCpssModel";
    return PredictionSource::kNoCpssModel;
  }

  if (request_type == permissions::RequestType::kGeolocation &&
      !is_geolocation_cpss_enabled) {
    VLOG(1) << "[CPSS] GetPredictionTypeToUse NoCpssModel";
    return PredictionSource::kNoCpssModel;
  }

  bool use_server_side = false;
  if (is_msbb_enabled) {
#if BUILDFLAG(IS_ANDROID)
    use_server_side = base::FeatureList::IsEnabled(
        permissions::features::kPermissionDedicatedCpssSettingAndroid);
#else
    use_server_side = base::FeatureList::IsEnabled(
        permissions::features::kPermissionPredictionsV2);
#endif  // BUILDFLAG(IS_ANDROID)
  }
  if (use_server_side) {
    // AIvX models take priority over each other in the following order:
    // AIv4, AIv3
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    if (PredictionModelHandlerProvider::IsAIv4FeatureEnabled()) {
      VLOG(1) << "[CPSS] GetPredictionTypeToUse AIv4";
      return PredictionSource::kOnDeviceAiv4AndServerSideModel;
    }
    if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv3)) {
      VLOG(1) << "[CPSS] GetPredictionTypeToUse AIv3";
      return PredictionSource::kOnDeviceAiv3AndServerSideModel;
    }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    VLOG(1) << "[CPSS] GetPredictionTypeToUse CPSSv3";
    return PredictionSource::kServerSideCpssV3Model;
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if ((request_type == permissions::RequestType::kNotifications &&
       base::FeatureList::IsEnabled(
           permissions::features::
               kPermissionOnDeviceNotificationPredictions)) ||
      (request_type == permissions::RequestType::kGeolocation &&
       base::FeatureList::IsEnabled(
           permissions::features::kPermissionOnDeviceGeolocationPredictions))) {
    VLOG(1) << "[CPSS] GetPredictionTypeToUse CPSSv1";
    return PredictionSource::kOnDeviceCpssV1Model;
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  VLOG(1) << "[CPSS] GetPredictionTypeToUse NoCpssModel";
  return PredictionSource::kNoCpssModel;
}

void PermissionsAiUiSelector::set_language_detection_observer_for_testing(
    std::unique_ptr<permissions::LanguageDetectionObserver>
        language_detection_observer) {
  CHECK_IS_TEST();
  language_detection_observer_ = std::move(language_detection_observer);
}

void PermissionsAiUiSelector::set_inner_text_for_testing(
    content_extraction::InnerTextResult inner_text_) {
  CHECK_IS_TEST();
  inner_text_for_testing_ = std::move(inner_text_);
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PermissionsAiUiSelector::set_snapshot_for_testing(SkBitmap snapshot) {
  CHECK_IS_TEST();
  snapshot_for_testing_ = snapshot;
}

void PermissionsAiUiSelector::TakeSnapshot(
    content::RenderWidgetHostView* host_view,
    ModelExecutionData model_data) {
  VLOG(1) << "[PermissionsAIvX] TakeSnapshot";
  auto snapshot_inquire_start_time = base::TimeTicks::Now();
  if (snapshot_for_testing_.has_value()) {
    OnSnapshotTakenForOnDeviceModel(snapshot_inquire_start_time,
                                    std::move(model_data),
                                    snapshot_for_testing_.value());
  } else if (!host_view) {
    VLOG(1) << "[CPSS] Snapshot cannot be taken because host_view is nullptr.";
    FinishRequest(Decision::UseNormalUiAndShowNoWarning());
  } else {
    host_view->CopyFromSurface(
        gfx::Rect(), gfx::Size(),
        base::BindOnce([](const viz::CopyOutputBitmapWithMetadata& result) {
          return result.bitmap;
        })
            .Then(base::BindOnce(
                &PermissionsAiUiSelector::OnSnapshotTakenForOnDeviceModel,
                weak_ptr_factory_.GetWeakPtr(), snapshot_inquire_start_time,
                std::move(model_data))));
  }
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

void PermissionsAiUiSelector::GetInnerText(
    content::RenderFrameHost* render_frame_host,
    ModelExecutionData model_data,
    ModelExecutionCallback model_execution_callback) {
  VLOG(1) << "[PermissionsAI] GetInnerText";
  if (inner_text_for_testing_.has_value()) {
    return OnGetInnerTextForOnDeviceModel(
        std::move(model_data), std::move(model_execution_callback),
        std::make_unique<content_extraction::InnerTextResult>(
            std::move(inner_text_for_testing_.value())));
  }
  content_extraction::GetInnerText(
      *render_frame_host, /*node_id=*/std::nullopt,
      base::BindOnce(&PermissionsAiUiSelector::OnGetInnerTextForOnDeviceModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(model_data),
                     std::move(model_execution_callback)));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PermissionsAiUiSelector::ExecuteOnDeviceAivXModel(
    ModelExecutionData model_data) {
  VLOG(1) << "[PermissionsAI] ExecuteOnDeviceAivXModel";
  PredictionModelHandlerProvider* prediction_model_handler_provider =
      PredictionModelHandlerProviderFactory::GetForBrowserContext(profile_);
  if (prediction_model_handler_provider) {
    permissions::RequestType request_type =
        model_data.request_metadata.request_type;

    switch (model_data.model_type) {
      case PredictionModelType::kOnDeviceAiV3Model: {
        DCHECK(model_data.snapshot.has_value());
        VLOG(1)
            << "[PermissionsAI] ExecuteOnDeviceAivXModel kOnDeviceAiV3Model";
        if (PermissionsAiv3Handler* aiv3_handler =
                prediction_model_handler_provider->GetPermissionsAiv3Handler(
                    request_type)) {
          VLOG(1) << "[PermissionsAI] Inquire AIv3 model";
          return aiv3_handler->ExecuteModel(
              /*callback=*/base::BindOnce(
                  &PermissionsAiUiSelector::
                      OnDeviceTfliteAivXModelExecutionCallback,
                  weak_ptr_factory_.GetWeakPtr(),
                  /*model_inquire_start_time=*/base::TimeTicks::Now(),
                  std::move(model_data.features),
                  std::move(model_data.request_metadata),
                  model_data.model_type),
              /*model_input=*/PermissionsAiv3Handler::ModelInput(
                  std::move(model_data.snapshot.value())));
        } else {
          VLOG(1) << "[PermissionsAI] No AIv3 handler";
        }
        break;
      }
      case PredictionModelType::kOnDeviceAiV4Model: {
        DCHECK(model_data.snapshot.has_value());
        DCHECK(model_data.inner_text_embedding.has_value());
        if (PermissionsAiv4Handler* aiv4_handler =
                prediction_model_handler_provider->GetPermissionsAiv4Handler(
                    request_type)) {
          VLOG(1) << "[PermissionsAIv4] Inquire model";
          return aiv4_handler->ExecuteModel(
              /*callback=*/base::BindOnce(
                  &PermissionsAiUiSelector::
                      OnDeviceTfliteAivXModelExecutionCallback,
                  weak_ptr_factory_.GetWeakPtr(),
                  /*model_inquire_start_time=*/base::TimeTicks::Now(),
                  std::move(model_data.features),
                  std::move(model_data.request_metadata),
                  model_data.model_type),
              /*model_input=*/PermissionsAiv4Handler::ModelInput(
                  std::move(model_data.snapshot.value()),
                  std::move(model_data.inner_text_embedding.value())));
        }
        break;
      }
      default:
        NOTREACHED();
    }
  } else {
    VLOG(1) << "[PermissionsAIvX] On device AI model session unavailable";
  }
#endif

  InquireServerModel(model_data.features,
                     std::move(model_data.request_metadata));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PermissionsAiUiSelector::OnPassageEmbeddingsComputed(
    ModelExecutionData model_data,
    ModelExecutionCallback model_execution_callback,
    passage_embeddings::Embedding embedding) {
  VLOG(1) << "[PermissionsAIv4] OnPassageEmbeddingsComputed";
  model_data.inner_text_embedding = std::move(embedding);
  std::move(model_execution_callback).Run(std::move(model_data));
}
#endif
