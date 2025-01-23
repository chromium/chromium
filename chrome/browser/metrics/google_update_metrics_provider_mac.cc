// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/google_update_metrics_provider_mac.h"

#include <optional>

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/updater/update_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

GoogleUpdateMetricsProviderMac::GoogleUpdateMetricsProviderMac() = default;
GoogleUpdateMetricsProviderMac::~GoogleUpdateMetricsProviderMac() = default;

void GoogleUpdateMetricsProviderMac::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  metrics::SystemProfileProto::GoogleUpdate* google_update =
      system_profile_proto->mutable_google_update();

  std::optional<updater::UpdateService::AppState> browser_state =
      BrowserUpdaterClient::GetLastKnownBrowserRegistration();
  if (browser_state) {
    base::UmaHistogramSparse("GoogleUpdate.InstallDetails.UpdateCohortId",
                             base::PersistentHash(browser_state->cohort.substr(
                                 0, browser_state->cohort.find_last_of(":"))));
    google_update->mutable_client_status()->set_version(
        browser_state->version.GetString());
  }

  std::optional<updater::UpdateService::AppState> updater_state =
      BrowserUpdaterClient::GetLastKnownUpdaterRegistration();
  if (updater_state) {
    google_update->mutable_google_update_status()->set_version(
        updater_state->version.GetString());
  }
}
