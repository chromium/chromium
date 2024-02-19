// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/test/test_extensions_client.h"

namespace extensions {

using FeatureProviderBrowserTest = InProcessBrowserTest;

// This browser test collects all of the features in the extensions system.  The
// test determines from a hardcoded list, provided via
// |GetExpectedDelegatedFeaturesForTest()|, that for every feature in the
// extensions system whether we expect the feature to require and have a
// delegated check set that the feature's settings match those expectations.
// This ensures correct Feature functionality that translates json settings into
// our system settings.
IN_PROC_BROWSER_TEST_F(FeatureProviderBrowserTest,
                       VerifyRequiresDelegatedAvailabilityCheckFeatures) {
  const std::vector<const char*> expected_delegated_features =
      extension_test_util::GetExpectedDelegatedFeaturesForTest();
  const FeatureProvider* api_provider = FeatureProvider::GetAPIFeatures();
  const FeatureMap& feature_map = api_provider->GetAllFeatures();
  for (const auto& it : feature_map) {
    bool is_delegated_feature =
        base::Contains(expected_delegated_features, it.first);
    const Feature* feature = it.second.get();
    ASSERT_TRUE(feature);
    EXPECT_EQ(is_delegated_feature,
              feature->RequiresDelegatedAvailabilityCheck());
    EXPECT_EQ(is_delegated_feature,
              feature->HasDelegatedAvailabilityCheckHandlerForTesting());
  }
}

}  // namespace extensions
