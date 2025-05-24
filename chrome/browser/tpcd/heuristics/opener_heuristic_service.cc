// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_service.h"

#include "base/functional/bind.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_service_factory.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "url/origin.h"

OpenerHeuristicService::OpenerHeuristicService(
    base::PassKey<OpenerHeuristicServiceFactory>,
    content::BrowserContext* context)
    : browser_context_(context),
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
  tracking_protection_settings_ = nullptr;
  tracking_protection_settings_observation_.Reset();
}

void OpenerHeuristicService::OnTrackingProtection3pcdChanged() {
  if (!tracking_protection_settings_ ||
      !tracking_protection_settings_->IsTrackingProtection3pcdEnabled()) {
    NotifyBackfillPopupHeuristicGrants(false);
    return;
  }

  browser_context_->BackfillPopupHeuristicGrants(base::BindOnce(
      &OpenerHeuristicService::NotifyBackfillPopupHeuristicGrants,
      weak_factory_.GetWeakPtr()));
}

void OpenerHeuristicService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OpenerHeuristicService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OpenerHeuristicService::NotifyBackfillPopupHeuristicGrants(bool success) {
  for (auto& observer : observers_) {
    observer.OnBackfillPopupHeuristicGrants(browser_context_.get(), success);
  }
}
