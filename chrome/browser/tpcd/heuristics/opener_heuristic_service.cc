// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_service.h"

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/dips/dips_service_impl.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_service_factory.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "url/origin.h"

OpenerHeuristicService::OpenerHeuristicService(
    base::PassKey<OpenerHeuristicServiceFactory>,
    content::BrowserContext* context)
    : browser_context_(context),
      dips_(DIPSServiceImpl::Get(context)),
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
  tracking_protection_settings_ = nullptr;
  tracking_protection_settings_observation_.Reset();
}

void OpenerHeuristicService::OnTrackingProtection3pcdChanged() {
  if (IsShuttingDown()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(
          content_settings::features::kTpcdHeuristicsGrants) ||
      !tpcd::experiment::kTpcdBackfillPopupHeuristicsGrants.Get()
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

    // `popup_site` and `opener_site` were read from the DIPS database,
    // and were originally computed by calling GetSiteForDIPS().
    // GrantCookieAccessDueToHeuristic() takes SchemefulSites, so we create some
    // here, but since we pass ignore_schemes=true the scheme doesn't matter
    // (and port never matters for SchemefulSites), so we hardcode http and 80.
    net::SchemefulSite popup_site(
        url::Origin::CreateFromNormalizedTuple("http", popup.popup_site, 80));
    net::SchemefulSite opener_site(
        url::Origin::CreateFromNormalizedTuple("http", popup.opener_site, 80));

    // TODO: crbug.com/40883201 - When we move to //content, we will call
    // this via ContentBrowserClient instead of as a standalone function.
    dips_move::GrantCookieAccessDueToHeuristic(browser_context_, opener_site,
                                               popup_site, grant_duration,
                                               /*ignore_schemes=*/true);
  }
}
