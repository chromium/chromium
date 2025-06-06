// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/default_clock.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/permissions_aiv1_handler.h"
#include "chrome/browser/permissions/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_model_handler_provider_factory.h"
#include "chrome/browser/permissions/prediction_service_factory.h"
#include "chrome/browser/permissions/prediction_service_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/features/permissions_ai.pb.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_service.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace {

using ::permissions::PermissionRequest;
using ::permissions::PermissionRequestRelevance;
using ::permissions::PermissionsAiv1Handler;
using ::permissions::PermissionsAiv3Handler;
using ::permissions::PermissionUmaUtil;
using ::permissions::PredictionModelHandlerProvider;
using ::permissions::PredictionModelType;
using ::permissions::PredictionRequestFeatures;
using QuietUiReason = PredictionBasedPermissionUiSelector::QuietUiReason;
using Decision = PredictionBasedPermissionUiSelector::Decision;
using PredictionSource = PredictionBasedPermissionUiSelector::PredictionSource;
using ::optimization_guide::proto::PermissionsAiResponse;

constexpr auto VeryUnlikely = permissions::
    PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;

// The data we consider can only be at most 28 days old to match the data that
// the ML model is built on.
constexpr base::TimeDelta kPermissionActionCutoffAge = base::Days(28);

// Only send requests if there are at least 4 action in the user's history for
// the particular permission type.
constexpr size_t kRequestedPermissionMinimumHistoricalActions = 4;

// The maximum length of a page's content. It is needed to limit on-device ML
// input to reduce processing latency.
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
    PermissionUmaUtil::PredictionGrantLikelihood likelihood) {
  return likelihood == VeryUnlikely;
}

void LogSnapshotTakenSuccessfullyForAiv3(bool success) {
  base::UmaHistogramBoolean("Permissions.AIv3.SnapshotTaken", success);
}

void LogPredictionModelHandlerProviderForAiv3(bool exists) {
  base::UmaHistogramBoolean("Permissions.AIv3.ModelHandlerProviderExists",
                            exists);
}

void LogPermissionsAiv3HandlerForAiv3(bool exists) {
  base::UmaHistogramBoolean("Permissions.AIv3.PermissionsAiv3HandlerExists",
                            exists);
}

void LogAiv3RelevanceHasValue(bool has_value) {
  base::UmaHistogramBoolean("Permissions.AIv3.RelevanceHasValue",
    has_value);
}

}  // namespace

PredictionBasedPermissionUiSelector::PredictionBasedPermissionUiSelector(
    Profile* profile)
    : profile_(profile) {
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

PredictionBasedPermissionUiSelector::~PredictionBasedPermissionUiSelector() =
    default;

void PredictionBasedPermissionUiSelector::InquireServerModel(
    const PredictionRequestFeatures& features,
    PredictionRequestMetadata request_metadata,
    bool record_source) {
  permissions::PredictionService* service =
      PredictionServiceFactory::GetForProfile(profile_);

  VLOG(1) << "[CPSS] Starting prediction service request";

  if (record_source) {
    PermissionUmaUtil::RecordPermissionPredictionSource(
        permissions::PermissionPredictionSource::SERVER_SIDE);
  }

  request_ = std::make_unique<PredictionServiceRequest>(
      service, features,
      base::BindOnce(
          &PredictionBasedPermissionUiSelector::LookupResponseReceived,
          base::Unretained(this),
          /*model_inquire_start_time=*/base::TimeTicks::Now(),
          std::move(request_metadata)));
}

void PredictionBasedPermissionUiSelector::
    InquireOnDeviceAiv1AndServerModelIfAvailable(
        content::RenderFrameHost* rfh,
        PredictionRequestFeatures features,
        PredictionRequestMetadata request_metadata) {
  content_extraction::GetInnerText(
      *rfh, /*node_id=*/std::nullopt,
      base::BindOnce(
          &PredictionBasedPermissionUiSelector::OnGetInnerTextForOnDeviceModel,
          weak_ptr_factory_.GetWeakPtr(), std::move(features),
          std::move(request_metadata)));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PredictionBasedPermissionUiSelector::InquireCpssV1OnDeviceModelIfAvailable(
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
  if (prediction_model_handler && prediction_model_handler->ModelAvailable()) {
    VLOG(1) << "[CPSS] Using locally available CPSSv1 model";
    PermissionUmaUtil::RecordPermissionPredictionSource(
        permissions::PermissionPredictionSource::ON_DEVICE_TFLITE);
    auto proto_request = GetPredictionRequestProto(features);
    cpss_v1_model_holdback_probability_ =
        prediction_model_handler->HoldBackProbability();
    prediction_model_handler->ExecuteModelWithMetadata(
        base::BindOnce(
            &PredictionBasedPermissionUiSelector::LookupResponseReceived,
            weak_ptr_factory_.GetWeakPtr(),
            /*model_inquire_start_time=*/base::TimeTicks::Now(),
            std::move(request_metadata),
            /*lookup_succesful=*/true, /*response_from_cache=*/false),
        std::move(proto_request));
    return;
  }
  VLOG(1) << "[CPSS] On device CPSSv1 model unavailable";
  std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
}

void PredictionBasedPermissionUiSelector::
    InquireOnDeviceAiv3AndServerModelIfAvailable(
        content::RenderWidgetHostView* host_view,
        PredictionRequestFeatures features,
        PredictionRequestMetadata request_metadata) {
  if (!host_view) {
    VLOG(1) << "[CPSS] On device AIv3 model unavailable";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  if (snapshot_for_testing_.has_value()) {
    PredictionBasedPermissionUiSelector::OnSnapshotTakenForOnDeviceModel(
        std::move(features), std::move(request_metadata),
        snapshot_for_testing_.value());
    return;
  }

  // TODO(crbug.com/382447738) Add time measurement metrics
  host_view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindOnce(
          &PredictionBasedPermissionUiSelector::OnSnapshotTakenForOnDeviceModel,
          weak_ptr_factory_.GetWeakPtr(), std::move(features),
          std::move(request_metadata)));
}

void PredictionBasedPermissionUiSelector::OnSnapshotTakenForOnDeviceModel(
    PredictionRequestFeatures features,
    PredictionRequestMetadata request_metadata,
    const SkBitmap& snapshot) {
  VLOG(1) << "[PermissionsAIv3] On device AI prediction requested";
  LogSnapshotTakenSuccessfullyForAiv3(/*success=*/!snapshot.drawsNothing());
  if (snapshot.drawsNothing()) {
    VLOG(1) << "[PermissionsAIv3] The page's snapshot is empty";
  } else {
    PredictionModelHandlerProvider* prediction_model_handler_provider =
        PredictionModelHandlerProviderFactory::GetForBrowserContext(profile_);
    LogPredictionModelHandlerProviderForAiv3(
        prediction_model_handler_provider != nullptr);
    if (prediction_model_handler_provider) {
      PermissionsAiv3Handler* aiv3_handler =
          prediction_model_handler_provider->GetPermissionsAiv3Handler(
              request_metadata.request_type);

      LogPermissionsAiv3HandlerForAiv3(aiv3_handler != nullptr);
      if (aiv3_handler) {
        VLOG(1) << "[PermissionsAIv3] Inquire model";

        aiv3_handler->ExecuteModel(
            base::BindRepeating(
                &PredictionBasedPermissionUiSelector::
                    OnDeviceAiv3ModelExecutionCallback,
                weak_ptr_factory_.GetWeakPtr(),
                /*model_inquire_start_time=*/base::TimeTicks::Now(),
                std::move(features), std::move(request_metadata)),
            std::make_unique<SkBitmap>(snapshot));
        return;
      }
    }
    VLOG(1) << "[PermissionsAIv3] On device AI model session unavailable";
  }
  InquireServerModel(features, std::move(request_metadata),
                     /*record_source=*/true);
}

void PredictionBasedPermissionUiSelector::OnDeviceAiv3ModelExecutionCallback(
    base::TimeTicks model_inquire_start_time,
    PredictionRequestFeatures features,
    PredictionRequestMetadata request_metadata,
    const std::optional<PermissionRequestRelevance>& relevance) {
  PermissionUmaUtil::RecordPredictionModelInquireTime(
      model_inquire_start_time, PredictionModelType::kOnDeviceAiV3Model);
  VLOG(1) << "[PermissionsAIv3]: AI model execution callback called "
          << (relevance.has_value() ? "with value" : "without value");
  LogAiv3RelevanceHasValue(relevance.has_value());
  if (relevance.has_value()) {
    VLOG(1) << "[PermissionsAIv3]: PermissionRequest has a relevance of "
            << static_cast<int>(relevance.value());
    last_permission_request_relevance_ = relevance.value();
    features.permission_relevance = relevance.value();
    base::UmaHistogramEnumeration("Permissions.AIv3.PermissionRequestRelevance",
          features.permission_relevance);
  } else {
    last_permission_request_relevance_ =
        PermissionRequestRelevance::kUnspecified;
  }

  // We get Unspecified only if the model was not executed; so we call the
  // server side model as if we never inquired the on-device model before.
  InquireServerModel(features, std::move(request_metadata),
                     /*record_source=*/
                     !(relevance == PermissionRequestRelevance::kUnspecified));
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

void PredictionBasedPermissionUiSelector::SelectUiToUse(
    content::WebContents* web_contents,
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  VLOG(1) << "[CPSS] Selector activated";
  callback_ = std::move(callback);
  last_permission_request_relevance_ = std::nullopt;
  last_request_grant_likelihood_ = std::nullopt;
  cpss_v1_model_holdback_probability_ = std::nullopt;
  was_decision_held_back_ = std::nullopt;

  const PredictionSource prediction_source =
      GetPredictionTypeToUse(request->request_type());
  if (prediction_source == PredictionSource::kNoCpssModel) {
    VLOG(1) << "[CPSS] Configuration does not allow CPSS requests";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  PredictionRequestFeatures features = BuildPredictionRequestFeatures(request);
  if (prediction_source == PredictionSource::kOnDeviceCpssV1Model) {
    if (features.requested_permission_counts.total() <
        kRequestedPermissionMinimumHistoricalActions) {
      VLOG(1) << "[CPSS] Historic prompt count ("
              << features.requested_permission_counts.total()
              << ") is smaller than threshold ("
              << kRequestedPermissionMinimumHistoricalActions << ")";
      std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
      return;
    }
  }

  if (likelihood_override_for_testing_.has_value()) {
    VLOG(1) << "[CPSS] Using likelihood override value that was provided via "
               "command line";
    if (ShouldPredictionTriggerQuietUi(
            likelihood_override_for_testing_.value())) {
      std::move(callback_).Run(
          Decision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                   Decision::ShowNoWarning()));
    } else {
      std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    }
    return;
  }

  DCHECK(!request_);
  PredictionRequestMetadata request_metadata = {
      .prediction_source = prediction_source,
      .request_type = request->request_type()};

  switch (prediction_source) {
    case PredictionSource::kServerSideCpssV3Model:
      return InquireServerModel(features, std::move(request_metadata),
                                /*record_source=*/true);
    case PredictionSource::kOnDeviceAiv1AndServerSideModel:
      return InquireOnDeviceAiv1AndServerModelIfAvailable(
          web_contents->GetPrimaryMainFrame(), std::move(features),
          std::move(request_metadata));
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
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
      std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
      return;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    case PredictionSource::kNoCpssModel:
      [[fallthrough]];
    default:
      NOTREACHED();
  }
}

void PredictionBasedPermissionUiSelector::OnGetInnerTextForOnDeviceModel(
    PredictionRequestFeatures features,
    PredictionRequestMetadata request_metadata,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  VLOG(1) << "[PermissionsAIv1] On device AI prediction requested";
  if (result && result->inner_text.size() > kPageContentMinLength) {
    std::string inner_text = std::move(result->inner_text);

    if (inner_text.size() > kPageContentMaxLength) {
      inner_text.resize(kPageContentMaxLength);
    }
    if (PredictionModelHandlerProvider* prediction_model_handler_provider =
            PredictionModelHandlerProviderFactory::GetForBrowserContext(
                profile_)) {
      if (PermissionsAiv1Handler* aiv1_handler =
              prediction_model_handler_provider->GetPermissionsAiv1Handler()) {
        VLOG(1) << "[PermissionsAIv1] Inquire model";
        aiv1_handler->InquireAiOnDeviceModel(
            std::move(inner_text), request_metadata.request_type,
            base::BindRepeating(&PredictionBasedPermissionUiSelector::
                                    OnDeviceAiv1ModelExecutionCallback,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(features),
                                std::move(request_metadata)));
        return;
      }
    }
    VLOG(1) << "[PermissionsAIv1] On device AI model session unavailable";
  } else {
    VLOG(1) << "[PermissionsAIv1] The page's content is too short or empty";
  }
  InquireServerModel(features, std::move(request_metadata),
                     /*record_source=*/true);
}

void PredictionBasedPermissionUiSelector::Cancel() {
  request_.reset();
  callback_.Reset();
}

bool PredictionBasedPermissionUiSelector::IsPermissionRequestSupported(
    permissions::RequestType request_type) {
  return request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation;
}

std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
PredictionBasedPermissionUiSelector::PredictedGrantLikelihoodForUKM() {
  return last_request_grant_likelihood_;
}

std::optional<PermissionRequestRelevance>
PredictionBasedPermissionUiSelector::PermissionRequestRelevanceForUKM() {
  return last_permission_request_relevance_;
}

std::optional<bool>
PredictionBasedPermissionUiSelector::WasSelectorDecisionHeldback() {
  return was_decision_held_back_;
}

PredictionRequestFeatures
PredictionBasedPermissionUiSelector::BuildPredictionRequestFeatures(
    PermissionRequest* request) {
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

  features.experiment_id =
      PredictionRequestFeatures::ExperimentId::kNoExperimentId;
  bool use_aiv1 =
      base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv1);
  bool use_aiv3 =
      base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv3) ||
      base::FeatureList::IsEnabled(
          permissions::features::kPermissionsAIv3Geolocation);
  if (use_aiv1 || use_aiv3) {
    // Init `permission_relevance` here to avoid a crash during
    // `ConvertToProtoRelevance` execution.
    features.permission_relevance = PermissionRequestRelevance::kUnspecified;
    features.experiment_id =
        use_aiv1 ? PredictionRequestFeatures::ExperimentId::kAiV1ExperimentId
                 : PredictionRequestFeatures::ExperimentId::kAiV3ExperimentId;
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

void PredictionBasedPermissionUiSelector::OnDeviceAiv1ModelExecutionCallback(
    PredictionRequestFeatures features,
    PredictionRequestMetadata request_metadata,
    std::optional<PermissionsAiResponse> response) {
  VLOG(1) << "[PermissionsAIv1]: AI model execution callback called "
          << (response.has_value() ? "with value" : "without value");
  if (response.has_value()) {
    last_permission_request_relevance_ =
        response.value().is_permission_relevant()
            ? PermissionRequestRelevance::kVeryHigh
            : PermissionRequestRelevance::kVeryLow;
    VLOG(1) << "[PermissionsAIv1]: Permission request is "
            << (response.value().is_permission_relevant() ? "relevant"
                                                          : "not relevant");
    PermissionUmaUtil::RecordPermissionPredictionSource(
        permissions::PermissionPredictionSource::ONDEVICE_AI_AND_SERVER_SIDE);
  } else {
    last_permission_request_relevance_ =
        PermissionRequestRelevance::kUnspecified;
  }
  features.permission_relevance = last_permission_request_relevance_.value();
  PermissionUmaUtil::RecordPermissionRequestRelevance(
      features.permission_relevance);
  InquireServerModel(features, std::move(request_metadata),
                     /*record_source=*/!response.has_value());
}

void PredictionBasedPermissionUiSelector::LookupResponseReceived(
    base::TimeTicks model_inquire_start_time,
    PredictionRequestMetadata request_metadata,
    bool lookup_successful,
    bool response_from_cache,
    const std::optional<permissions::GeneratePredictionsResponse>& response) {
  // This function is used as callback for request to the CPSSv1 on-device model
  // and the CPSSv3 server-side model. As we have multiple prediction sources
  // that use the server side model in the end, we check for the CPSSv1 here and
  // set is_on_device depending on this.
  bool is_on_device_cpss_v1 = request_metadata.prediction_source ==
                              PredictionSource::kOnDeviceCpssV1Model;
  PermissionUmaUtil::RecordPredictionModelInquireTime(
      model_inquire_start_time,
      is_on_device_cpss_v1 ? PredictionModelType::kOnDeviceCpssV1Model
                           : PredictionModelType::kServerSideCpssV3Model);

  request_.reset();
  if (!callback_) {
    VLOG(1) << "[CPSS] Prediction service response ignored as the request is "
               "canceled";
    return;
  }
  if (!lookup_successful || !response || response->prediction_size() == 0) {
    VLOG(1) << "[CPSS] Prediction service request failed";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  last_request_grant_likelihood_ =
      response->prediction(0).grant_likelihood().discretized_likelihood();

  if (ShouldHoldBack(request_metadata)) {
    VLOG(1) << "[CPSS] Prediction service decision held back";
    was_decision_held_back_ = true;
    std::move(callback_).Run(
        Decision(Decision::UseNormalUi(), Decision::ShowNoWarning()));
    return;
  }
  was_decision_held_back_ = false;
  VLOG(1) << "[CPSS] Prediction service request succeeded and received "
             "likelihood: "
          << last_request_grant_likelihood_.value();

  if (ShouldPredictionTriggerQuietUi(last_request_grant_likelihood_.value())) {
    std::move(callback_).Run(
        Decision(is_on_device_cpss_v1
                     ? QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant
                     : QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                 Decision::ShowNoWarning()));
    return;
  }

  std::move(callback_).Run(
      Decision(Decision::UseNormalUi(), Decision::ShowNoWarning()));
}

bool PredictionBasedPermissionUiSelector::ShouldHoldBack(
    const PredictionRequestMetadata& request_metadata) const {
  permissions::RequestType request_type = request_metadata.request_type;
  PredictionSource prediction_source = request_metadata.prediction_source;
  DCHECK(request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation);

  // Holdback probability for this request.
  const double holdback_chance = base::RandDouble();
  bool should_holdback = false;
  PredictionModelType prediction_model =
      PredictionModelType::kServerSideCpssV3Model;

  switch (prediction_source) {
    case PredictionSource::kOnDeviceCpssV1Model:
      DCHECK(cpss_v1_model_holdback_probability_.has_value());
      should_holdback = holdback_chance < *cpss_v1_model_holdback_probability_;
      prediction_model = PredictionModelType::kOnDeviceCpssV1Model;
      break;
      // For on-device model + server-side model requests we will use the
      // holdback logic for the server-side model execution.
    case PredictionSource::kOnDeviceAiv3AndServerSideModel:
      prediction_model = PredictionModelType::kOnDeviceAiV3Model;
      [[fallthrough]];
    case PredictionSource::kOnDeviceAiv1AndServerSideModel:
      // We don't analyse holdback UMA results separately for aiv1, so we
      // don't set the model type for this one.
      [[fallthrough]];
    case PredictionSource::kServerSideCpssV3Model:
      should_holdback =
          holdback_chance <
          permissions::feature_params::kPermissionPredictionsV2HoldbackChance
              .Get();
      break;
    default:
      NOTREACHED();
  }
  PermissionUmaUtil::RecordPermissionPredictionServiceHoldback(
      request_type, prediction_model, should_holdback);
  return should_holdback;
}

PredictionSource PredictionBasedPermissionUiSelector::GetPredictionTypeToUse(
    permissions::RequestType request_type) {
  const bool is_msbb_enabled = profile_->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);

  const bool is_notification_cpss_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kEnableNotificationCPSS);

  const bool is_geolocation_cpss_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kEnableGeolocationCPSS);

  if (request_type == permissions::RequestType::kNotifications &&
      !is_notification_cpss_enabled) {
    return PredictionSource::kNoCpssModel;
  }

  if (request_type == permissions::RequestType::kGeolocation &&
      !is_geolocation_cpss_enabled) {
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
    // Aiv3 takes priority over Aiv1 if both are enabled.
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    if (request_type == permissions::RequestType::kNotifications &&
        base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv3)) {
      return PredictionSource::kOnDeviceAiv3AndServerSideModel;
    }
    if (request_type == permissions::RequestType::kGeolocation &&
        base::FeatureList::IsEnabled(
            permissions::features::kPermissionsAIv3Geolocation)) {
      return PredictionSource::kOnDeviceAiv3AndServerSideModel;
    }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv1)) {
      return PredictionSource::kOnDeviceAiv1AndServerSideModel;
    }
    return PredictionSource::kServerSideCpssV3Model;
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  bool use_ondevice_tflite = false;
  if (request_type == permissions::RequestType::kNotifications) {
    use_ondevice_tflite = base::FeatureList::IsEnabled(
        permissions::features::kPermissionOnDeviceNotificationPredictions);
  } else if (request_type == permissions::RequestType::kGeolocation) {
    use_ondevice_tflite = base::FeatureList::IsEnabled(
        permissions::features::kPermissionOnDeviceGeolocationPredictions);
  }
  if (use_ondevice_tflite) {
    return PredictionSource::kOnDeviceCpssV1Model;
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  return PredictionSource::kNoCpssModel;
}

void PredictionBasedPermissionUiSelector::set_snapshot_for_testing(
    SkBitmap snapshot) {
  CHECK_IS_TEST();
  snapshot_for_testing_ = snapshot;
}
