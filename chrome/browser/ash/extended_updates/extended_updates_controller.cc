// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/ownership/owner_settings_service.h"

namespace ash {

namespace {

// Returns true if the user has the ability to opt in the device.
bool HasOptInAbility(ownership::OwnerSettingsService* owner_settings) {
  // Only owner user can opt in.
  // By extension, only unmanaged devices can opt in.
  if (!owner_settings || !owner_settings->IsOwner()) {
    return false;
  }

  // Check feature enablement after other checks to reduce noise due to how
  // finch experiment is recorded.
  if (!ash::features::IsExtendedUpdatesOptInFeatureEnabled()) {
    return false;
  }

  // Only eligible if not already opted in.
  return !IsExtendedUpdatesOptedIn();
}

}  // namespace

bool IsExtendedUpdatesOptInEligible(content::BrowserContext* context,
                                    const ExtendedUpdatesParams& params) {
  // Valid date range is between extended date and eol date.
  // Extended date is expected to be before eol date.
  // Also, not eligible if opt-in is not required.
  if (params.eol_passed || !params.extended_date_passed ||
      !params.opt_in_required) {
    return false;
  }

  return IsExtendedUpdatesOptInEligible(context);
}

bool IsExtendedUpdatesOptInEligible(content::BrowserContext* context) {
  auto* owner_settings =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(context);
  return HasOptInAbility(owner_settings);
}

bool IsExtendedUpdatesOptedIn() {
  bool value;
  if (CrosSettings::Get()->GetBoolean(kDeviceExtendedAutoUpdateEnabled,
                                      &value)) {
    return value;
  }
  return false;
}

bool OptInExtendedUpdates(content::BrowserContext* context) {
  auto* owner_settings =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(context);
  if (!HasOptInAbility(owner_settings)) {
    return false;
  }

  // TODO(b/329513970): Add metrics.
  return owner_settings->SetBoolean(kDeviceExtendedAutoUpdateEnabled, true);
}

}  // namespace ash
