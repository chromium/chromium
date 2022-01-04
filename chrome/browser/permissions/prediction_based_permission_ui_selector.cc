// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
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
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_service.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/permissions/prediction_model_handler_factory.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace {

using QuietUiReason = PredictionBasedPermissionUiSelector::QuietUiReason;
using Decision = PredictionBasedPermissionUiSelector::Decision;

constexpr auto VeryUnlikely = permissions::
    PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;

// The data we consider can only be at most 28 days old to match the data that
// the ML model is built on.
constexpr base::TimeDelta kPermissionActionCutoffAge = base::Days(28);

// Only send requests if there are at least 4 action in the user's history for
// the particular permission type.
constexpr size_t kRequestedPermissionMinimumHistoricalActions = 4;

absl::optional<
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

  return absl::nullopt;
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
    if (mock_likelihood.has_value())
      set_likelihood_override(mock_likelihood.value());
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDevicePredictions)) {
    PredictionModelHandlerFactory::GetForBrowserContext(profile);
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

PredictionBasedPermissionUiSelector::~PredictionBasedPermissionUiSelector() =
    default;

void PredictionBasedPermissionUiSelector::SelectUiToUse(
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  VLOG(1) << "[CPSS] Selector activated";
  callback_ = std::move(callback);
  last_request_grant_likelihood_ = absl::nullopt;

  if (!IsAllowedToUseAssistedPrompts(request->request_type())) {
    VLOG(1) << "[CPSS] Configuration either does not allows CPSS requests or "
               "the request was held back";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  auto features = BuildPredictionRequestFeatures(request);
  if (features.requested_permission_counts.total() <
      kRequestedPermissionMinimumHistoricalActions) {
    VLOG(1) << "[CPSS] Historic prompt count ("
            << features.requested_permission_counts.total()
            << ") is smaller than threshold ("
            << kRequestedPermissionMinimumHistoricalActions << ")";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  if (likelihood_override_for_testing_.has_value()) {
    VLOG(1) << "[CPSS] Using likelihood override value that was provided via "
               "command line";
    if (ShouldPredictionTriggerQuietUi(
            likelihood_override_for_testing_.value())) {
      std::move(callback_).Run(
          Decision(QuietUiReason::kPredictedVeryUnlikelyGrant,
                   Decision::ShowNoWarning()));
    } else {
      std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    }
    return;
  }

  DCHECK(!request_);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDevicePredictions)) {
    permissions::PredictionModelHandler* prediction_model_handler =
        PredictionModelHandlerFactory::GetForBrowserContext(profile_);
    if (prediction_model_handler->ModelAvailable()) {
      VLOG(1) << "[CPSS] Using locally available model";
      auto proto_request = GetPredictionRequestProto(features);
      prediction_model_handler->ExecuteModelWithInput(
          base::BindOnce(
              &PredictionBasedPermissionUiSelector::LookupResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), /*is_on_device=*/true,
              /*lookup_succesful=*/true, /*response_from_cache=*/false),
          *proto_request);
      return;
    }
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  permissions::PredictionService* service =
      PredictionServiceFactory::GetForProfile(profile_);

  VLOG(1) << "[CPSS] Starting prediction service request";
  request_ = std::make_unique<PredictionServiceRequest>(
      service, features,
      base::BindOnce(
          &PredictionBasedPermissionUiSelector::LookupResponseReceived,
          base::Unretained(this), /*is_on_device=*/false));
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

absl::optional<permissions::PermissionUmaUtil::PredictionGrantLikelihood>
PredictionBasedPermissionUiSelector::PredictedGrantLikelihoodForUKM() {
  return last_request_grant_likelihood_;
}

permissions::PredictionRequestFeatures
PredictionBasedPermissionUiSelector::BuildPredictionRequestFeatures(
    permissions::PermissionRequest* request) {
  permissions::PredictionRequestFeatures features;
  features.gesture = request->GetGestureType();
  features.type = request->request_type();

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
    bool lookup_succesful,
    bool response_from_cache,
    const absl::optional<permissions::GeneratePredictionsResponse>& response) {
  request_.reset();
  if (!lookup_succesful || !response || response->prediction_size() == 0) {
    VLOG(1) << "[CPSS] Prediction service request failed";
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  last_request_grant_likelihood_ =
      response->prediction(0).grant_likelihood().discretized_likelihood();

  VLOG(1)
      << "[CPSS] Prediction service request succeeded and received likelihood: "
      << last_request_grant_likelihood_.value();

  if (ShouldPredictionTriggerQuietUi(last_request_grant_likelihood_.value())) {
    std::move(callback_).Run(Decision(
        QuietUiReason::kPredictedVeryUnlikelyGrant, Decision::ShowNoWarning()));
    return;
  }

  std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
}

bool PredictionBasedPermissionUiSelector::IsAllowedToUseAssistedPrompts(
    permissions::RequestType request_type) {
  // We need to also check `kQuietNotificationPrompts` here since there is no
  // generic safeguard anywhere else in the stack.
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts) ||
      !safe_browsing::IsSafeBrowsingEnabled(*(profile_->GetPrefs()))) {
    return false;
  }
  double hold_back_chance = 0.0;
  bool is_permissions_predictions_enabled = false;
  switch (request_type) {
    case permissions::RequestType::kNotifications:
      is_permissions_predictions_enabled =
          base::FeatureList::IsEnabled(features::kPermissionPredictions);
      hold_back_chance = features::kPermissionPredictionsHoldbackChance.Get();
      break;
    case permissions::RequestType::kGeolocation:
      // Only quiet chip ui is supported for Geolocation
      is_permissions_predictions_enabled =
          base::FeatureList::IsEnabled(
              features::kPermissionGeolocationPredictions) &&
          base::FeatureList::IsEnabled(
              permissions::features::kPermissionQuietChip);
      hold_back_chance =
          features::kPermissionGeolocationPredictionsHoldbackChance.Get();
      break;
    default:
      NOTREACHED();
  }
  if (!is_permissions_predictions_enabled)
    return false;

  const bool should_hold_back =
      hold_back_chance && base::RandDouble() < hold_back_chance;
  // Only recording the hold back UMA histogram if the request was actually
  // eligible for an assisted prompt
  switch (request_type) {
    case permissions::RequestType::kNotifications:
      base::UmaHistogramBoolean("Permissions.PredictionService.Request",
                                !should_hold_back);
      break;
    case permissions::RequestType::kGeolocation:
      base::UmaHistogramBoolean(
          "Permissions.PredictionService.GeolocationRequest",
          !should_hold_back);
      break;
    default:
      NOTREACHED();
  }
  return !should_hold_back;
}
