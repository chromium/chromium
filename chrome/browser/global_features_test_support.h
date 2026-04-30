// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_FEATURES_TEST_SUPPORT_H_
#define CHROME_BROWSER_GLOBAL_FEATURES_TEST_SUPPORT_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/global_features.h"

namespace test {

// A scoped helper to replace GlobalFeatures for testing.
// On construction, it replaces the global factory with the provided one.
// On destruction, it resets the factory to default.
class ScopedGlobalFeaturesOverride {
 public:
  explicit ScopedGlobalFeaturesOverride(
      GlobalFeatures::GlobalFeaturesFactory factory) {
    GlobalFeatures::ReplaceGlobalFeaturesForTesting(std::move(factory));
  }

  ScopedGlobalFeaturesOverride(const ScopedGlobalFeaturesOverride&) = delete;
  ScopedGlobalFeaturesOverride& operator=(const ScopedGlobalFeaturesOverride&) =
      delete;

  ~ScopedGlobalFeaturesOverride() {
    GlobalFeatures::ReplaceGlobalFeaturesForTesting(
        GlobalFeatures::GlobalFeaturesFactory());
  }
};

}  // namespace test

#endif  // CHROME_BROWSER_GLOBAL_FEATURES_TEST_SUPPORT_H_
