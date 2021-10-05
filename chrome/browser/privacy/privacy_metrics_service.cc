// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/privacy_metrics_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"

PrivacyMetricsService::PrivacyMetricsService(PrefService* pref_service)
    : pref_service_(pref_service) {
  RecordStartupMetrics();
}

PrivacyMetricsService::~PrivacyMetricsService() = default;

void PrivacyMetricsService::RecordStartupMetrics() {
  base::UmaHistogramBoolean(
      "Privacy.DoNotTrackSetting",
      pref_service_->GetBoolean(prefs::kEnableDoNotTrack));

  auto preload_setting_status =
      static_cast<chrome_browser_net::NetworkPredictionOptions>(
          pref_service_->GetInteger(::prefs::kNetworkPredictionOptions));
  base::UmaHistogramBoolean(
      "Settings.PreloadStatus.OnStartup",
      (preload_setting_status != chrome_browser_net::NETWORK_PREDICTION_NEVER));

  base::UmaHistogramBoolean(
      "Settings.AutocompleteSearches.OnStartup",
      pref_service_->GetBoolean(::prefs::kSearchSuggestEnabled));

  base::UmaHistogramBoolean(
      "Settings.AdvancedSpellcheck.OnStartup",
      pref_service_->GetBoolean(
          ::spellcheck::prefs::kSpellCheckUseSpellingService));
}
