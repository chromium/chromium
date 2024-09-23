// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"

namespace {

using QuietUiReason = ContextualNotificationPermissionUiSelector::QuietUiReason;
using WarningReason = ContextualNotificationPermissionUiSelector::WarningReason;
using Decision = ContextualNotificationPermissionUiSelector::Decision;

// Records a histogram sample for NotificationUserExperienceQuality.
void RecordNotificationUserExperienceQuality(
    CrowdDenyPreloadData::SiteReputation::NotificationUserExperienceQuality
        value) {
  // Cannot use either base::UmaHistogramEnumeration() overload here because
  // ARRAYSIZE is defined as MAX+1 but also as type `int`.
  base::UmaHistogramExactLinear(
      "Permissions.CrowdDeny.PreloadData.NotificationUxQuality",
      static_cast<int>(value),
      CrowdDenyPreloadData::SiteReputation::
          NotificationUserExperienceQuality_ARRAYSIZE);
}

// Attempts to decide which UI to use based on preloaded site reputation data,
// or returns std::nullopt if not possible. |site_reputation| can be nullptr.
std::optional<Decision> GetDecisionBasedOnSiteReputation(
    const CrowdDenyPreloadData::SiteReputation* site_reputation) {
  using Config = QuietNotificationPermissionUiConfig;
  if (!site_reputation) {
    RecordNotificationUserExperienceQuality(
        CrowdDenyPreloadData::SiteReputation::UNKNOWN);
    return std::nullopt;
  }

  RecordNotificationUserExperienceQuality(
      site_reputation->notification_ux_quality());

  switch (site_reputation->notification_ux_quality()) {
    case CrowdDenyPreloadData::SiteReputation::ACCEPTABLE: {
      return Decision::UseNormalUiAndShowNoWarning();
    }
    case CrowdDenyPreloadData::SiteReputation::UNSOLICITED_PROMPTS: {
      if (site_reputation->warning_only())
        return Decision::UseNormalUiAndShowNoWarning();
      if (!Config::IsCrowdDenyTriggeringEnabled())
        return std::nullopt;
      return Decision(QuietUiReason::kTriggeredByCrowdDeny,
                      Decision::ShowNoWarning());
    }
    case CrowdDenyPreloadData::SiteReputation::ABUSIVE_PROMPTS: {
      if (site_reputation->warning_only()) {
        if (!Config::IsAbusiveRequestWarningEnabled())
          return Decision::UseNormalUiAndShowNoWarning();
        return Decision(Decision::UseNormalUi(),
                        WarningReason::kAbusiveRequests);
      }
      if (!Config::IsAbusiveRequestBlockingEnabled())
        return std::nullopt;
      return Decision(QuietUiReason::kTriggeredDueToAbusiveRequests,
                      Decision::ShowNoWarning());
    }
    case CrowdDenyPreloadData::SiteReputation::ABUSIVE_CONTENT: {
      if (site_reputation->warning_only()) {
        if (!Config::IsAbusiveContentTriggeredRequestWarningEnabled())
          return Decision::UseNormalUiAndShowNoWarning();
        return Decision(Decision::UseNormalUi(),
                        WarningReason::kAbusiveContent);
      }
      if (!Config::IsAbusiveContentTriggeredRequestBlockingEnabled())
        return std::nullopt;
      return Decision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      Decision::ShowNoWarning());
    }
    case CrowdDenyPreloadData::SiteReputation::DISRUPTIVE_BEHAVIOR: {
      DCHECK(!site_reputation->warning_only());

      if (!Config::IsDisruptiveBehaviorRequestBlockingEnabled())
        return std::nullopt;
      return Decision(QuietUiReason::kTriggeredDueToDisruptiveBehavior,
                      Decision::ShowNoWarning());
    }
    case CrowdDenyPreloadData::SiteReputation::UNKNOWN: {
      return std::nullopt;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

// Roll the dice to decide whether to use the normal UI even when the preload
// data indicates that quiet UI should be used. This creates a control group of
// normal UI prompt impressions, which facilitates comparing acceptance rates,
// better calibrating server-side logic, and detecting when the notification
// experience on the site has improved.
bool ShouldHoldBackQuietUI(QuietUiReason quiet_ui_reason) {
  const double kHoldbackChance =
      QuietNotificationPermissionUiConfig::GetCrowdDenyHoldBackChance();

  // There is no hold-back when the quiet UI is shown due to abusive permission
  // request UX, as those verdicts are not calculated based on acceptance rates.
  if (quiet_ui_reason != QuietUiReason::kTriggeredByCrowdDeny)
    return false;

  // Avoid rolling a dice if the chance is 0.
  const bool result = kHoldbackChance && base::RandDouble() < kHoldbackChance;
  base::UmaHistogramBoolean("Permissions.CrowdDeny.DidHoldbackQuietUi", result);
  return result;
}

}  // namespace

ContextualNotificationPermissionUiSelector::
    ContextualNotificationPermissionUiSelector() = default;

void ContextualNotificationPermissionUiSelector::SelectUiToUse(
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  callback_ = std::move(callback);
  DCHECK(callback_);

  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts)) {
    Notify(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  // Even if the quiet UI is enabled on all sites, the crowd deny, abuse and
  // disruption trigger conditions must be evaluated first, so that the
  // corresponding, less prominent UI and the strings are shown on blocklisted
  // origins.
  EvaluatePerSiteTriggers(url::Origin::Create(request->requesting_origin()));
}

void ContextualNotificationPermissionUiSelector::Cancel() {
  // The computation either finishes synchronously above, or is waiting on the
  // Safe Browsing check.
  safe_browsing_request_.reset();
}

bool ContextualNotificationPermissionUiSelector::IsPermissionRequestSupported(
    permissions::RequestType request_type) {
  return request_type == permissions::RequestType::kNotifications;
}

ContextualNotificationPermissionUiSelector::
    ~ContextualNotificationPermissionUiSelector() = default;

void ContextualNotificationPermissionUiSelector::EvaluatePerSiteTriggers(
    const url::Origin& origin) {
  CrowdDenyPreloadData::GetInstance()->GetReputationDataForSiteAsync(
      origin,
      base::BindOnce(
          &ContextualNotificationPermissionUiSelector::OnSiteReputationReady,
          weak_factory_.GetWeakPtr(), origin));
}

void ContextualNotificationPermissionUiSelector::OnSiteReputationReady(
    const url::Origin& origin,
    const CrowdDenyPreloadData::SiteReputation* reputation) {
  std::optional<Decision> decision =
      GetDecisionBasedOnSiteReputation(reputation);

  // If the PreloadData suggests this is an unacceptable site, ping Safe
  // Browsing to verify; but do not ping if it is not warranted.
  if (!decision || (!decision->quiet_ui_reason && !decision->warning_reason)) {
    Notify(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  DCHECK(!safe_browsing_request_);
  DCHECK(g_browser_process->safe_browsing_service());

  // It is fine to use base::Unretained() here, as |safe_browsing_request_|
  // guarantees not to fire the callback after its destruction.
  safe_browsing_request_.emplace(
      g_browser_process->safe_browsing_service()->database_manager(),
      base::DefaultClock::GetInstance(), origin,
      base::BindOnce(&ContextualNotificationPermissionUiSelector::
                         OnSafeBrowsingVerdictReceived,
                     base::Unretained(this), *decision));
}

void ContextualNotificationPermissionUiSelector::OnSafeBrowsingVerdictReceived(
    Decision candidate_decision,
    CrowdDenySafeBrowsingRequest::Verdict verdict) {
  DCHECK(safe_browsing_request_);
  DCHECK(callback_);

  safe_browsing_request_.reset();

  switch (verdict) {
    case CrowdDenySafeBrowsingRequest::Verdict::kAcceptable:
      Notify(Decision::UseNormalUiAndShowNoWarning());
      break;
    case CrowdDenySafeBrowsingRequest::Verdict::kUnacceptable:
      if (candidate_decision.quiet_ui_reason &&
          ShouldHoldBackQuietUI(*(candidate_decision.quiet_ui_reason))) {
        candidate_decision.quiet_ui_reason.reset();
      }
      Notify(candidate_decision);
      break;
  }
}

void ContextualNotificationPermissionUiSelector::Notify(
    const Decision& decision) {
  std::move(callback_).Run(decision);
}

// static
