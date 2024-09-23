// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/time/default_clock.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/prediction_service_factory.h"
#include "chrome/browser/permissions/prediction_service_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
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

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/permissions/prediction_model_handler_provider_factory.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#include "components/permissions/prediction_service/prediction_model_handler_provider.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace {

using QuietUiReason = PredictionBasedPermissionUiSelector::QuietUiReason;
using Decision = PredictionBasedPermissionUiSelector::Decision;
using PredictionSource = PredictionBasedPermissionUiSelector::PredictionSource;

constexpr auto VeryUnlikely = permissions::
    PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;

// The data we consider can only be at most 28 days old to match the data that
// the ML model is built on.
constexpr base::TimeDelta kPermissionActionCutoffAge = base::Days(28);

// Only send requests if there are at least 4 action in the user's history for
// the particular permission type.
constexpr size_t kRequestedPermissionMinimumHistoricalActions = 4;

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

void PredictionBasedPermissionUiSelector::SelectUiToUse(
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  VLOG(1) << "[CPSS] Selector activated";
  callback_ = std::move(callback);
  last_request_grant_likelihood_ = std::nullopt;
  was_decision_held_back_ = std::nullopt;
  const PredictionSource prediction_source =
      GetPredictionTypeToUse(request->request_type());
  if (prediction_source == PredictionSource::USE_NONE) {
    VLOG(1) << "[CPSS] Configuration does not allow CPSS requests";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  auto features = BuildPredictionRequestFeatures(request);
  if (!base::FeatureList::IsEnabled(
          permissions::features::kPermissionPredictionsV3) ||
      prediction_source == PredictionSource::USE_ONDEVICE) {
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

  if (prediction_source == PredictionSource::USE_SERVER_SIDE) {
    permissions::PredictionService* service =
        PredictionServiceFactory::GetForProfile(profile_);

    VLOG(1) << "[CPSS] Starting prediction service request";
    permissions::PermissionUmaUtil::RecordPermissionPredictionSource(
        permissions::PermissionPredictionSource::SERVER_SIDE);
    request_ = std::make_unique<PredictionServiceRequest>(
        service, features,
        base::BindOnce(
            &PredictionBasedPermissionUiSelector::LookupResponseReceived,
            base::Unretained(this), /*is_on_device=*/false,
            request->request_type()));
    return;
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (prediction_source == PredictionSource::USE_ONDEVICE) {
    permissions::PredictionModelHandlerProvider*
        prediction_model_handler_provider =
            PredictionModelHandlerProviderFactory::GetForBrowserContext(
                profile_);
    permissions::PredictionModelHandler* prediction_model_handler = nullptr;
    if (prediction_model_handler_provider) {
      prediction_model_handler =
          prediction_model_handler_provider->GetPredictionModelHandler(
              request->request_type());
    }
    if (prediction_model_handler &&
        prediction_model_handler->ModelAvailable()) {
      VLOG(1) << "[CPSS] Using locally available model";
      permissions::PermissionUmaUtil::RecordPermissionPredictionSource(
          permissions::PermissionPredictionSource::ON_DEVICE);
      auto proto_request = GetPredictionRequestProto(features);
      prediction_model_handler->ExecuteModelWithMetadata(
          base::BindOnce(
              &PredictionBasedPermissionUiSelector::LookupResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), /*is_on_device=*/true,
              request->request_type(),
              /*lookup_succesful=*/true, /*response_from_cache=*/false),
          std::move(proto_request));
      return;
    } else {
      VLOG(1) << "[CPSS] On device model unavailable";
      std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
      return;
    }
  }
#else
  if (prediction_source == PredictionSource::USE_ONDEVICE) {
    VLOG(1) << "[CPSS] Client doesnt support tflite";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  NOTREACHED_IN_MIGRATION();
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

std::optional<bool>
PredictionBasedPermissionUiSelector::WasSelectorDecisionHeldback() {
  return was_decision_held_back_;
}

permissions::PredictionRequestFeatures
PredictionBasedPermissionUiSelector::BuildPredictionRequestFeatures(
    permissions::PermissionRequest* request) {
  permissions::PredictionRequestFeatures features;
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

  features.experiment_id = base::FeatureList::IsEnabled(
                               permissions::features::kPermissionPredictionsV3)
                               ? 1
                               : 0;

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

void PredictionBasedPermissionUiSelector::LookupResponseReceived(
    bool is_on_device,
    permissions::RequestType request_type,
    bool lookup_succesful,
    bool response_from_cache,
    const std::optional<permissions::GeneratePredictionsResponse>& response) {
  request_.reset();
  if (!callback_) {
    VLOG(1) << "[CPSS] Prediction service response ignored as the request is "
               "canceled";
    return;
  }
  if (!lookup_succesful || !response || response->prediction_size() == 0) {
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
  // Different holdback threshold for the different experiments.
  const double on_device_geolocation_holdback_threshold =
      permissions::feature_params::
          kPermissionOnDeviceGeolocationPredictionsHoldbackChance.Get();
  const double on_device_notification_holdback_threshold =
      permissions::feature_params::
          kPermissionOnDeviceNotificationPredictionsHoldbackChance.Get();
  const double server_side_holdback_threshold =
      permissions::feature_params::kPermissionPredictionsV2HoldbackChance.Get();

  // Holdback probability for this request.
  const double holdback_chance = base::RandDouble();
  bool should_holdback = false;
  if (is_on_device) {
    if (request_type == permissions::RequestType::kNotifications) {
      should_holdback =
          holdback_chance < on_device_notification_holdback_threshold;
    } else if (request_type == permissions::RequestType::kGeolocation) {
      should_holdback =
          holdback_chance < on_device_geolocation_holdback_threshold;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  } else {
    should_holdback = holdback_chance < server_side_holdback_threshold;
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

  bool is_tflite_available = false;
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  is_tflite_available = true;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  bool is_on_device_enabled = false;

  if (request_type == permissions::RequestType::kNotifications) {
    is_on_device_enabled = base::FeatureList::IsEnabled(
        permissions::features::kPermissionOnDeviceNotificationPredictions);
  } else if (request_type == permissions::RequestType::kGeolocation) {
    is_on_device_enabled = base::FeatureList::IsEnabled(
        permissions::features::kPermissionOnDeviceGeolocationPredictions);
  }
#if BUILDFLAG(IS_ANDROID)
  if (is_msbb_enabled &&
      base::FeatureList::IsEnabled(
          permissions::features::kPermissionDedicatedCpssSettingAndroid)) {
#else
  if (is_msbb_enabled && base::FeatureList::IsEnabled(
                             permissions::features::kPermissionPredictionsV2)) {
#endif
    return PredictionSource::USE_SERVER_SIDE;
  } else if (is_tflite_available && is_on_device_enabled) {
    return PredictionSource::USE_ONDEVICE;
  } else {
    return PredictionSource::USE_NONE;
  }
}
