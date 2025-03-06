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
#include "chrome/browser/permissions/permissions_ai_handler.h"
#include "chrome/browser/permissions/prediction_model_handler_provider.h"
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
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_service.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/permissions/prediction_model_handler_provider_factory.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace {

using ::permissions::PermissionRequest;
using ::permissions::PermissionsAiHandler;
using ::permissions::PredictionModelHandlerProvider;
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
    permissions::PermissionUmaUtil::PredictionGrantLikelihood likelihood) {
  return likelihood == VeryUnlikely;
}

void LogModelInquireTime(base::TimeTicks model_inquire_start_time,
                         bool is_on_device) {
  using permissions::PredictionModelType;

  std::string histogram_name =
      base::StrCat({"Permissions.",
                    permissions::PermissionUmaUtil::GetPredictionModelString(
                        is_on_device ? PredictionModelType::kTfLiteOnDevice
                                     : PredictionModelType::kServerSide),
                    ".InquiryDuration"});
  base::UmaHistogramMediumTimes(
      histogram_name, base::TimeTicks::Now() - model_inquire_start_time);
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
    permissions::RequestType request_type,
    bool record_source) {
  permissions::PredictionService* service =
      PredictionServiceFactory::GetForProfile(profile_);

  VLOG(1) << "[CPSS] Starting prediction service request";

  if (record_source) {
    permissions::PermissionUmaUtil::RecordPermissionPredictionSource(
        permissions::PermissionPredictionSource::SERVER_SIDE);
  }

  request_ = std::make_unique<PredictionServiceRequest>(
      service, features,
      base::BindOnce(
          &PredictionBasedPermissionUiSelector::LookupResponseReceived,
          base::Unretained(this),
          /*model_inquire_start_time=*/base::TimeTicks::Now(),
          /*is_on_device=*/false, request_type));
}

void PredictionBasedPermissionUiSelector::InquireTfliteOnDeviceModelIfAvailable(
    const PredictionRequestFeatures& features,
    permissions::RequestType request_type) {
#if !BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  VLOG(1) << "[CPSS] Client doesn't support tflite";
  std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
  return;
#else
  PredictionModelHandlerProvider* prediction_model_handler_provider =
      PredictionModelHandlerProviderFactory::GetForBrowserContext(profile_);
  permissions::PredictionModelHandler* prediction_model_handler = nullptr;
  if (prediction_model_handler_provider) {
    prediction_model_handler =
        prediction_model_handler_provider->GetPredictionModelHandler(
            request_type);
  }
  if (prediction_model_handler && prediction_model_handler->ModelAvailable()) {
    VLOG(1) << "[CPSS] Using locally available TFLite model";
    permissions::PermissionUmaUtil::RecordPermissionPredictionSource(
        permissions::PermissionPredictionSource::ON_DEVICE_TFLITE);
    auto proto_request = GetPredictionRequestProto(features);
    tflite_model_holdback_probability_ =
        prediction_model_handler->HoldBackProbability();
    prediction_model_handler->ExecuteModelWithMetadata(
        base::BindOnce(
            &PredictionBasedPermissionUiSelector::LookupResponseReceived,
            weak_ptr_factory_.GetWeakPtr(),
            /*model_inquire_start_time=*/base::TimeTicks::Now(),
            /*is_on_device=*/true, request_type,
            /*lookup_succesful=*/true, /*response_from_cache=*/false),
        std::move(proto_request));
    return;
  }
  VLOG(1) << "[CPSS] On device TFLite model unavailable";
  std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

void PredictionBasedPermissionUiSelector::
    InquireAiOnDeviceAndServerModelIfAvailable(
        content::RenderFrameHost* rfh,
        PredictionRequestFeatures features,
        permissions::RequestType request_type) {
  content_extraction::GetInnerText(
      *rfh, /*node_id=*/std::nullopt,
      base::BindOnce(
          &PredictionBasedPermissionUiSelector::OnGetInnerTextForOnDeviceModel,
          weak_ptr_factory_.GetWeakPtr(), std::move(features), request_type));
}

void PredictionBasedPermissionUiSelector::SelectUiToUse(
    content::WebContents* web_contents,
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  VLOG(1) << "[CPSS] Selector activated";
  callback_ = std::move(callback);
  last_permission_request_relevance_ = std::nullopt;
  last_request_grant_likelihood_ = std::nullopt;
  tflite_model_holdback_probability_ = std::nullopt;
  was_decision_held_back_ = std::nullopt;

  const PredictionSource prediction_source =
      GetPredictionTypeToUse(request->request_type());
  if (prediction_source == PredictionSource::USE_NONE) {
    VLOG(1) << "[CPSS] Configuration does not allow CPSS requests";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  PredictionRequestFeatures features = BuildPredictionRequestFeatures(request);
  if (prediction_source == PredictionSource::USE_ONDEVICE_TFLITE) {
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

  switch (prediction_source) {
    case PredictionSource::USE_ONDEVICE_AI_AND_SERVER_SIDE:
      InquireAiOnDeviceAndServerModelIfAvailable(
          web_contents->GetPrimaryMainFrame(), std::move(features),
          request->request_type());
      return;
    case PredictionSource::USE_SERVER_SIDE:
      return InquireServerModel(features, request->request_type(),
                                /*record_source=*/true);
    case PredictionSource::USE_ONDEVICE_TFLITE:
      return InquireTfliteOnDeviceModelIfAvailable(features,
                                                   request->request_type());
    default:
      NOTREACHED();
  }
}

void PredictionBasedPermissionUiSelector::OnGetInnerTextForOnDeviceModel(
    PredictionRequestFeatures features,
    permissions::RequestType request_type,
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
      if (PermissionsAiHandler* gen_ai_model_handler =
              prediction_model_handler_provider->GetPermissionsAiHandler()) {
        VLOG(1) << "[PermissionsAIv1] Inquire model.";
        gen_ai_model_handler->InquireAiOnDeviceModel(
            std::move(inner_text), request_type,
            base::BindRepeating(&PredictionBasedPermissionUiSelector::
                                    AiOnDeviceModelExecutionCallback,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(features), request_type));
        return;
      }
    }
    VLOG(1) << "[PermissionsAIv1] On device AI model session unavailable";
  } else {
    VLOG(1) << "[PermissionsAIv1] The page's contnet too short or empty";
  }
  InquireServerModel(features, request_type, /*record_source=*/true);
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

std::optional<permissions::PermissionUmaUtil::PredictionGrantLikelihood>
PredictionBasedPermissionUiSelector::PredictedGrantLikelihoodForUKM() {
  return last_request_grant_likelihood_;
}

std::optional<permissions::PermissionRequestRelevance>
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

  features.experiment_id = 0;

  if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv1)) {
    // Init `permission_relevance` here to avoid a crash during
    // `ConvertToProtoRelevance` execution.
    features.permission_relevance =
        permissions::PermissionRequestRelevance::kUnspecified;
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

void PredictionBasedPermissionUiSelector::AiOnDeviceModelExecutionCallback(
    PredictionRequestFeatures features,
    permissions::RequestType request_type,
    std::optional<PermissionsAiResponse> response) {
  VLOG(1) << "[PermissionsAIv1]: AI model execution callback called "
          << (response.has_value() ? "with value" : "without value");
  if (response.has_value()) {
    last_permission_request_relevance_ =
        response.value().is_permission_relevant()
            ? permissions::PermissionRequestRelevance::kVeryHigh
            : permissions::PermissionRequestRelevance::kVeryLow;
    VLOG(1) << "[PermissionsAIv1]: Permission request is "
            << (response.value().is_permission_relevant() ? "relevant"
                                                          : "not relevant");
    permissions::PermissionUmaUtil::RecordPermissionPredictionSource(
        permissions::PermissionPredictionSource::ONDEVICE_AI_AND_SERVER_SIDE);
  } else {
    last_permission_request_relevance_ =
        permissions::PermissionRequestRelevance::kUnspecified;
  }
  features.permission_relevance = last_permission_request_relevance_.value();
  InquireServerModel(features, request_type,
                     /*record_source=*/!response.has_value());
}

void PredictionBasedPermissionUiSelector::LookupResponseReceived(
    base::TimeTicks model_inquire_start_time,
    bool is_on_device,
    permissions::RequestType request_type,
    bool lookup_successful,
    bool response_from_cache,
    const std::optional<permissions::GeneratePredictionsResponse>& response) {
  LogModelInquireTime(model_inquire_start_time, is_on_device);

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

  if (ShouldHoldBack(is_on_device, request_type)) {
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
    std::move(callback_).Run(Decision(
        is_on_device ? QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant
                     : QuietUiReason::kServicePredictedVeryUnlikelyGrant,
        Decision::ShowNoWarning()));
    return;
  }

  std::move(callback_).Run(
      Decision(Decision::UseNormalUi(), Decision::ShowNoWarning()));
}

bool PredictionBasedPermissionUiSelector::ShouldHoldBack(
    bool is_on_device,
    permissions::RequestType request_type) {
  DCHECK(request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation);

  // Holdback probability for this request.
  const double holdback_chance = base::RandDouble();
  bool should_holdback = false;
  if (is_on_device) {
    DCHECK(tflite_model_holdback_probability_.has_value());
    should_holdback = holdback_chance < *tflite_model_holdback_probability_;
  } else {
    should_holdback =
        holdback_chance <
        permissions::feature_params::kPermissionPredictionsV2HoldbackChance
            .Get();
  }
  permissions::PermissionUmaUtil::RecordPermissionPredictionServiceHoldback(
      request_type, is_on_device, should_holdback);
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
    return PredictionSource::USE_NONE;
  }

  if (request_type == permissions::RequestType::kGeolocation &&
      !is_geolocation_cpss_enabled) {
    return PredictionSource::USE_NONE;
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
    if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv1)) {
      return PredictionSource::USE_ONDEVICE_AI_AND_SERVER_SIDE;
    }
    return PredictionSource::USE_SERVER_SIDE;
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
    return PredictionSource::USE_ONDEVICE_TFLITE;
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  return PredictionSource::USE_NONE;
}
