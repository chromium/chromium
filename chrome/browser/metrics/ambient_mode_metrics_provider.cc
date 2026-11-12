// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/ambient_mode_metrics_provider.h"

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_mode_photo_source.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace {

using AmbientModePhotoSource = ash::ambient::AmbientModePhotoSource;

AmbientModePhotoSource GetAmbientModePhotoSourcePref(
    const PrefService& pref_service) {
  auto value =
      pref_service.GetInteger(ash::ambient::prefs::kAmbientModePhotoSourcePref);

  CHECK_LE(0, value, base::NotFatalUntil::M161);
  CHECK_GE(static_cast<int>(AmbientModePhotoSource::kMaxValue), value,
           base::NotFatalUntil::M161);

  return static_cast<AmbientModePhotoSource>(value);
}

}  // namespace

AmbientModeMetricsProvider::AmbientModeMetricsProvider() = default;
AmbientModeMetricsProvider::~AmbientModeMetricsProvider() = default;

void AmbientModeMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  auto* ambient_client = ash::AmbientClient::Get();
  if (!ambient_client || !ambient_client->IsAmbientModeAllowed()) {
    return;
  }

  // `IsAmbientModeAllowed()` returns false until the active user's profile is
  // created.
  const PrefService* pref_service =
      CHECK_DEREF(
          user_manager::UserManager::Get()->FindUser(
              CHECK_DEREF(
                  session_manager::SessionManager::Get()->GetActiveSession())
                  .account_id()))
          .GetProfilePrefs();
  CHECK(pref_service);

  if (!pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled)) {
    return;
  }

  base::UmaHistogramEnumeration("Ash.AmbientMode.PhotoSource",
                                GetAmbientModePhotoSourcePref(*pref_service));
}
