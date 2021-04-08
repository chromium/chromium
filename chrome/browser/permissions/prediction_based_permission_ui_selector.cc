// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/permissions/permission_actions_history.h"
#include "chrome/browser/permissions/prediction_service_factory.h"
#include "chrome/browser/permissions/prediction_service_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/prediction_service.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace {

using QuietUiReason = PredictionBasedPermissionUiSelector::QuietUiReason;
using Decision = PredictionBasedPermissionUiSelector::Decision;

constexpr auto VeryUnlikely = permissions::
    PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;

// The data we consider can only be at most 28 days old to match the data that
// the ML model is built on.
constexpr base::TimeDelta kPermissionActionCutoffAge =
    base::TimeDelta::FromDays(28);

// Only send requests if there are at least 4 action in the user's history for
// the particular permission type.
constexpr size_t kRequestedPermissionMinimumHistoricalActions = 4;

base::Optional<
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

  return base::nullopt;
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
}

PredictionBasedPermissionUiSelector::~PredictionBasedPermissionUiSelector() =
    default;

void PredictionBasedPermissionUiSelector::SelectUiToUse(
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  VLOG(1) << "[CPSS] Selector activated";
  callback_ = std::move(callback);
  last_request_grant_likelihood_ = base::nullopt;

  if (!IsAllowedToUseAssistedPrompts()) {
    VLOG(1) << "[CPSS] Configuration does not allows CPSS requests";
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
  permissions::PredictionService* service =
      PredictionServiceFactory::GetForProfile(profile_);

  VLOG(1) << "[CPSS] Starting prediction service request";
  request_ = std::make_unique<PredictionServiceRequest>(
      service, features,
      base::BindOnce(
          &PredictionBasedPermissionUiSelector::LookupReponseReceived,
          base::Unretained(this)));
}

void PredictionBasedPermissionUiSelector::Cancel() {
  request_.reset();
  callback_.Reset();
}

base::Optional<permissions::PermissionUmaUtil::PredictionGrantLikelihood>
PredictionBasedPermissionUiSelector::PredictedGrantLikelihoodForUKM() {
  return last_request_grant_likelihood_;
}

permissions::PredictionRequestFeatures
PredictionBasedPermissionUiSelector::BuildPredictionRequestFeatures(
    permissions::PermissionRequest* request) {
  permissions::PredictionRequestFeatures features;
  features.gesture = request->GetGestureType();
  features.type = request->GetRequestType();

  base::Time cutoff = base::Time::Now() - kPermissionActionCutoffAge;

  auto* action_history = PermissionActionsHistory::GetForProfile(profile_);

  auto actions = action_history->GetHistory(
      cutoff, permissions::RequestType::kNotifications);
  FillInActionCounts(&features.requested_permission_counts, actions);

  actions = action_history->GetHistory(cutoff);
  FillInActionCounts(&features.all_permission_counts, actions);

  return features;
}

void PredictionBasedPermissionUiSelector::LookupReponseReceived(
    bool lookup_succesful,
    bool response_from_cache,
    std::unique_ptr<permissions::GeneratePredictionsResponse> response) {
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

bool PredictionBasedPermissionUiSelector::IsAllowedToUseAssistedPrompts() {
  // We need to also check `kQuietNotificationPrompts` here since there is no
  // generic safeguard anywhere else in the stack.
  return base::FeatureList::IsEnabled(features::kQuietNotificationPrompts) &&
         base::FeatureList::IsEnabled(features::kPermissionPredictions) &&
         safe_browsing::IsSafeBrowsingEnabled(*(profile_->GetPrefs()));
}

// static
void PredictionBasedPermissionUiSelector::FillInActionCounts(
    permissions::PredictionRequestFeatures::ActionCounts* counts,
    const std::vector<PermissionActionsHistory::Entry>& actions) {
  for (const auto& entry : actions) {
    switch (entry.action) {
      case permissions::PermissionAction::DENIED:
        counts->denies++;
        break;
      case permissions::PermissionAction::GRANTED:
        counts->grants++;
        break;
      case permissions::PermissionAction::DISMISSED:
        counts->dismissals++;
        break;
      case permissions::PermissionAction::IGNORED:
        counts->ignores++;
        break;
      default:
        // Anything else is ignored.
        break;
    }
  }
}
