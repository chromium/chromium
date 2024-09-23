// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_entropy_provider.h"

#include <string_view>

namespace base {

MockEntropyProvider::MockEntropyProvider() : entropy_value_(0.5) {}
MockEntropyProvider::MockEntropyProvider(double entropy_value)
    : entropy_value_(entropy_value) {}
MockEntropyProvider::~MockEntropyProvider() = default;

double MockEntropyProvider::GetEntropyForTrial(
    std::string_view trial_name,
    uint32_t randomization_seed) const {
  return entropy_value_;
}

}  // namespace base
