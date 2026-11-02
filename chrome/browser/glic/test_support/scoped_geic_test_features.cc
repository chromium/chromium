// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/scoped_geic_test_features.h"

#include <vector>

#include "chrome/browser/glic/public/features.h"

namespace glic {

ScopedGeicTestFeatures::ScopedGeicTestFeatures(bool enable_fre) {
  std::vector<base::test::FeatureRefAndParams> enabled_features = {
      {features::kGeic, {{features::kGeicGuestURL.name, kTestGeicGuestUrl}}}};
  if (enable_fre) {
    enabled_features.push_back({features::kGeicFre, {}});
  }
  feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
}

ScopedGeicTestFeatures::~ScopedGeicTestFeatures() = default;

}  // namespace glic
