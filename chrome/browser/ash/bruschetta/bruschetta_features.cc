// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"

namespace bruschetta {

static BruschettaFeatures* g_bruschetta_features = nullptr;

BruschettaFeatures* BruschettaFeatures::Get() {
  if (!g_bruschetta_features) {
    g_bruschetta_features = new BruschettaFeatures();
  }
  return g_bruschetta_features;
}

void BruschettaFeatures::SetForTesting(BruschettaFeatures* features) {
  g_bruschetta_features = features;
}

BruschettaFeatures::BruschettaFeatures() = default;

BruschettaFeatures::~BruschettaFeatures() = default;

bool BruschettaFeatures::IsEnabled() {
  return base::FeatureList::IsEnabled(ash::features::kBruschetta);
}

}  // namespace bruschetta
