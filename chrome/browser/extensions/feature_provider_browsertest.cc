// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include <algorithm>
#include <memory>

#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature_test_util.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/test/test_extensions_client.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

using FeatureProviderBrowserTest = PlatformBrowserTest;

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
  FeatureSpan features = api_provider->GetAllFeatures();
  for (const Feature* feature : features) {
    ASSERT_TRUE(feature);
    const std::string_view feature_name = feature->name();
    bool is_delegated_feature =
        std::ranges::contains(expected_delegated_features, feature_name);
    EXPECT_EQ(is_delegated_feature,
              feature->RequiresDelegatedAvailabilityCheck())
        << feature_name;
    EXPECT_EQ(is_delegated_feature,
              FeatureTestPeer::GetDelegatedAvailabilityCheckHandler(*feature) !=
                  nullptr)
        << feature_name;
  }
}

IN_PROC_BROWSER_TEST_F(FeatureProviderBrowserTest,
                       VerifyAPIJSONSourceHasFeatureTableEntries) {
  std::unique_ptr<JSONFeatureProviderSource> source =
      ExtensionsClient::Get()->CreateAPIFeatureSource();
  const FeatureProvider* api_provider = FeatureProvider::GetAPIFeatures();
  ASSERT_FALSE(source->dictionary().empty());
  for (const auto item : source->dictionary()) {
    const base::DictValue* feature_definition = item.second.GetIfDict();
    if (feature_definition) {
      if (feature_definition->FindBool("nocompile").value_or(false)) {
        EXPECT_FALSE(api_provider->GetFeature(item.first)) << item.first;
      } else {
        EXPECT_TRUE(api_provider->GetFeature(item.first)) << item.first;
      }
    } else {
      const base::ListValue* feature_definitions = item.second.GetIfList();
      ASSERT_TRUE(feature_definitions) << item.first;
      EXPECT_TRUE(api_provider->GetFeature(item.first)) << item.first;
    }
  }
}

}  // namespace
}  // namespace extensions
