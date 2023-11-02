// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/fake_bruschetta_features.h"

namespace bruschetta {

FakeBruschettaFeatures::FakeBruschettaFeatures() {
  original_features_ = BruschettaFeatures::Get();
  BruschettaFeatures::SetForTesting(this);
}

FakeBruschettaFeatures::~FakeBruschettaFeatures() {
  BruschettaFeatures::SetForTesting(original_features_);
}

void FakeBruschettaFeatures::SetAll(bool flag) {
  enabled_ = flag;
}

void FakeBruschettaFeatures::ClearAll() {
  enabled_ = absl::nullopt;
}

bool FakeBruschettaFeatures::IsEnabled() {
  if (enabled_.has_value())
    return *enabled_;
  return original_features_->IsEnabled();
}

}  // namespace bruschetta
