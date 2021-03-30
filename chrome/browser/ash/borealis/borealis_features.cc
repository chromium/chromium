// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"

#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/prefs/pref_service.h"

namespace borealis {

BorealisFeatures::BorealisFeatures(Profile* profile) : profile_(profile) {}

bool BorealisFeatures::IsAllowed() {
  if (!base::FeatureList::IsEnabled(features::kBorealis))
    return false;

  bool allowed_for_device;
  if (chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kBorealisAllowedForDevice, &allowed_for_device)) {
    if (!allowed_for_device)
      return false;
  }

  if (!profile_->GetPrefs()->GetBoolean(prefs::kBorealisAllowedForUser))
    return false;

  return true;
}

bool BorealisFeatures::IsEnabled() {
  if (!IsAllowed())
    return false;
  return profile_->GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice);
}

}  // namespace borealis
