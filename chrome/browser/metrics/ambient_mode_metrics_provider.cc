// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/ambient_mode_metrics_provider.h"

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"

AmbientModeMetricsProvider::AmbientModeMetricsProvider() = default;
AmbientModeMetricsProvider::~AmbientModeMetricsProvider() = default;

void AmbientModeMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!chromeos::features::IsAmbientModeEnabled() ||
      !ash::AmbientClient::Get()->IsAmbientModeAllowed()) {
    return;
  }

  base::UmaHistogramBoolean(
      "Ash.AmbientMode.Enabled",
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          ash::ambient::prefs::kAmbientModeEnabled));
}
