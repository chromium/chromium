// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_service.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"

OpenerHeuristicService::OpenerHeuristicService(
    base::PassKey<OpenerHeuristicServiceFactory>,
    content::BrowserContext* context)
    : dips_(DIPSService::Get(context)),
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(context))),
      tracking_protection_settings_(
          TrackingProtectionSettingsFactory::GetForProfile(
              Profile::FromBrowserContext(context))) {
  if (tracking_protection_settings_) {
    tracking_protection_settings_observation_.Observe(
        tracking_protection_settings_.get());
  }
}

OpenerHeuristicService::~OpenerHeuristicService() = default;

/* static */
OpenerHeuristicService* OpenerHeuristicService::Get(
    content::BrowserContext* context) {
  return OpenerHeuristicServiceFactory::GetForBrowserContext(context);
}

void OpenerHeuristicService::Shutdown() {
  dips_ = nullptr;
  cookie_settings_.reset();
  tracking_protection_settings_ = nullptr;
  tracking_protection_settings_observation_.Reset();
}

void OpenerHeuristicService::OnTrackingProtection3pcdChanged() {
  if (IsShuttingDown()) {
    return;
  }

  if (!tpcd::experiment::kTpcdBackfillPopupHeuristicsGrants.Get()
           .is_positive()) {
    return;
  }

  if (!tracking_protection_settings_ ||
      !tracking_protection_settings_->IsTrackingProtection3pcdEnabled()) {
    return;
  }

  // TODO: crbug.com/1502264 - ensure backfill is completed if interrupted
  dips_->storage()
      ->AsyncCall(&DIPSStorage::ReadRecentPopupsWithInteraction)
      .WithArgs(tpcd::experiment::kTpcdBackfillPopupHeuristicsGrants.Get())
      .Then(
          base::BindOnce(&OpenerHeuristicService::BackfillPopupHeuristicGrants,
                         weak_factory_.GetWeakPtr()));
}

void OpenerHeuristicService::BackfillPopupHeuristicGrants(
    std::vector<PopupWithTime> recent_popups) {
  if (IsShuttingDown()) {
    return;
  }

  for (const auto& popup : recent_popups) {
    base::TimeDelta grant_duration =
        tpcd::experiment::kTpcdBackfillPopupHeuristicsGrants.Get() -
        (base::Time::Now() - popup.last_popup_time);
    if (!grant_duration.is_positive()) {
      continue;
    }

    // Create cookie access grants scoped to the schemeless pattern, since the
    // scheme is not available.
    GURL popup_url = GURL(base::StrCat({"http://", popup.popup_site}));
    GURL opener_url = GURL(base::StrCat({"http://", popup.opener_site}));
    cookie_settings_->SetTemporaryCookieGrantForHeuristic(
        popup_url, opener_url, grant_duration,
        /*use_schemeless_patterns=*/true);
  }
}
