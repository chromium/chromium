// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/ambient_mode_metrics_provider.h"

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_mode_photo_source.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"

namespace {

using AmbientModePhotoSource = ash::ambient::AmbientModePhotoSource;

AmbientModePhotoSource GetAmbientModePhotoSourcePref(
    PrefService* pref_service) {
  auto value = pref_service->GetInteger(
      ash::ambient::prefs::kAmbientModePhotoSourcePref);

  DCHECK_LE(0, value);
  DCHECK_GE(static_cast<int>(AmbientModePhotoSource::kMaxValue), value);

  return static_cast<AmbientModePhotoSource>(value);
}

}  // namespace

AmbientModeMetricsProvider::AmbientModeMetricsProvider() = default;
AmbientModeMetricsProvider::~AmbientModeMetricsProvider() = default;

void AmbientModeMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  auto* ambient_client = ash::AmbientClient::Get();
  if (!ambient_client || !ambient_client->IsAmbientModeAllowed())
    return;

  // |IsAmbientModeAllowed| guarantees a valid profile exists for the active
  // user.
  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetPrefs();
  DCHECK(pref_service);

  bool enabled =
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled);

  base::UmaHistogramBoolean("Ash.AmbientMode.Enabled", enabled);

  if (!enabled)
    return;

  base::UmaHistogramEnumeration("Ash.AmbientMode.PhotoSource",
                                GetAmbientModePhotoSourcePref(pref_service));
}
