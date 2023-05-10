// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/notreached.h"

namespace base::android {

std::pair<StringPiece, const Feature*> MakeNameToFeaturePair(
    const Feature* feature) {
  return std::make_pair(feature->name, feature);
}

FeatureMap::FeatureMap(std::vector<const Feature*> features_exposed_to_java) {
  mapping_ = MakeFlatMap<StringPiece, const Feature*>(
      features_exposed_to_java, {}, &MakeNameToFeaturePair);
}

FeatureMap::~FeatureMap() = default;

const Feature* FeatureMap::FindFeatureExposedToJava(
    const StringPiece& feature_name) {
  auto it = mapping_.find(feature_name);
  if (it != mapping_.end()) {
    return it->second;
  }

  NOTREACHED_NORETURN() << "Queried feature cannot be found in FeatureMap: "
                        << feature_name;
}

}  // namespace base::android
