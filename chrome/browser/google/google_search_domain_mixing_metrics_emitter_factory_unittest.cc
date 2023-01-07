// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_domain_mixing_metrics_emitter_factory.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

class GoogleSearchDomainMixingMetricsEmitterFactoryTest : public testing::Test {
 protected:
  bool ServiceIsCreatedWithBrowserContext() {
    return GoogleSearchDomainMixingMetricsEmitterFactory::GetInstance()
        ->ServiceIsCreatedWithBrowserContext();
  }
};

TEST_F(GoogleSearchDomainMixingMetricsEmitterFactoryTest, DisabledByDefault) {
  EXPECT_FALSE(ServiceIsCreatedWithBrowserContext());
}

TEST_F(GoogleSearchDomainMixingMetricsEmitterFactoryTest, Enabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kEmitGoogleSearchDomainMixingMetrics);

  EXPECT_TRUE(ServiceIsCreatedWithBrowserContext());
}
