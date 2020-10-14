// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_features.h"

#include "chrome/browser/chromeos/borealis/borealis_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"

namespace borealis {

BorealisFeatures::BorealisFeatures(Profile* profile) : profile_(profile) {}

bool BorealisFeatures::IsAllowed() {
  return base::FeatureList::IsEnabled(features::kBorealis);
}

bool BorealisFeatures::IsEnabled() {
  if (!IsAllowed())
    return false;
  return profile_->GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice);
}

}  // namespace borealis
