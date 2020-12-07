// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/default_clock.h"
#include "base/util/values/values_util.h"
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
    PermissionSuggestion_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;

// The data we consider can only be at most 28 days old to match the data that
// the ML model is built on.
constexpr base::TimeDelta kPermissionActionCutoffAge =
    base::TimeDelta::FromDays(28);

constexpr char kPermissionActionEntryActionKey[] = "action";
constexpr char kPermissionActionEntryTimestampKey[] = "time";

base::Optional<
    permissions::PermissionSuggestion_Likelihood_DiscretizedLikelihood>
ParsePredictionServiceMockLikelihood(const std::string& value) {
  if (value == "very-unlikely") {
    return permissions::
        PermissionSuggestion_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;
  } else if (value == "unlikely") {
    return permissions::
        PermissionSuggestion_Likelihood_DiscretizedLikelihood_UNLIKELY;
  } else if (value == "neutral") {
    return permissions::
        PermissionSuggestion_Likelihood_DiscretizedLikelihood_NEUTRAL;
  } else if (value == "likely") {
    return permissions::
        PermissionSuggestion_Likelihood_DiscretizedLikelihood_LIKELY;
  } else if (value == "very-likely") {
    return permissions::
        PermissionSuggestion_Likelihood_DiscretizedLikelihood_VERY_LIKELY;
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
  if (!IsAllowedToUseAssistedPrompts()) {
    std::move(callback).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  if (likelihood_override_for_testing_.has_value()) {
    if (ShouldPredictionTriggerQuietUi(
            likelihood_override_for_testing_.value())) {
      std::move(callback).Run(
          Decision(QuietUiReason::kPredictedVeryUnlikelyGrant,
                   Decision::ShowNoWarning()));
    } else {
      std::move(callback).Run(Decision::UseNormalUiAndShowNoWarning());
    }
    return;
  }

  last_request_grant_likelihood_ = base::nullopt;

  DCHECK(!request_);
  permissions::PredictionService* service =
      PredictionServiceFactory::GetForProfile(profile_);
  callback_ = std::move(callback);
  request_ = std::make_unique<PredictionServiceRequest>(
      service, BuildPredictionRequestFeatures(request),
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
  features.type = request->GetPermissionRequestType();
  auto* permission_actions =
      profile_->GetPrefs()->GetList(prefs::kNotificationPermissionActions);

  base::Time cutoff = base::Time::Now() - kPermissionActionCutoffAge;
  for (const auto& action : *permission_actions) {
    const base::Optional<base::Time> timestamp =
        util::ValueToTime(action.FindKey(kPermissionActionEntryTimestampKey));

    if (!timestamp || *timestamp < cutoff)
      continue;

    const base::Optional<int> past_action_as_int =
        action.FindIntKey(kPermissionActionEntryActionKey);
    DCHECK(past_action_as_int);

    const permissions::PermissionAction past_action =
        static_cast<permissions::PermissionAction>(*past_action_as_int);

    // TODO(andypaicu): implement recording all prompts outcomes regardless of
    // type. We currently only count notification prompts.
    switch (past_action) {
      case permissions::PermissionAction::DENIED:
        features.requested_permission_counts.denies++;
        features.all_permission_counts.denies++;
        break;
      case permissions::PermissionAction::GRANTED:
        features.requested_permission_counts.grants++;
        features.all_permission_counts.grants++;
        break;
      case permissions::PermissionAction::DISMISSED:
        features.requested_permission_counts.dismissals++;
        features.all_permission_counts.dismissals++;
        break;
      case permissions::PermissionAction::IGNORED:
        features.requested_permission_counts.ignores++;
        features.all_permission_counts.ignores++;
        break;
      default:
        // Anything else is ignored.
        break;
    }
  }

  return features;
}

void PredictionBasedPermissionUiSelector::LookupReponseReceived(
    bool lookup_succesful,
    bool response_from_cache,
    std::unique_ptr<permissions::GetSuggestionsResponse> response) {
  request_.reset();
  if (!lookup_succesful || !response || response->suggestion_size() == 0) {
    std::move(callback_).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  last_request_grant_likelihood_ =
      response->suggestion(0).grant_likelihood().discretized_likelihood();

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
         safe_browsing::IsEnhancedProtectionEnabled(*(profile_->GetPrefs()));
}
