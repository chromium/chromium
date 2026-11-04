// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/scoped_geic_test_features.h"

#include "chrome/browser/glic/public/features.h"

namespace glic {

ScopedGeicTestFeatures::ScopedGeicTestFeatures() {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kGeic, {{features::kGeicGuestURL.name, kTestGeicGuestUrl}});
}

ScopedGeicTestFeatures::~ScopedGeicTestFeatures() = default;

}  // namespace glic
