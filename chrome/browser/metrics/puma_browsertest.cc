// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/testing/metrics_consent_override.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/country_codes/country_codes.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/metrics/private_metrics/puma_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "content/public/test/browser_test.h"

namespace metrics::private_metrics {

PumaService* GetPumaService() {
  return g_browser_process->GetMetricsServicesManager()->GetPumaService();
}

class PumaBrowserTest : public SyncTest {
 public:
  PumaBrowserTest() : SyncTest(SINGLE_CLIENT) {
    scoped_feature_list_.InitWithFeatures(
        {kPrivateMetricsPuma, kPrivateMetricsPumaRc}, {kPrivateMetricsFeature});
  }

  PumaBrowserTest(const PumaBrowserTest&) = delete;
  PumaBrowserTest& operator=(const PumaBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PumaBrowserTest, GetCountryId) {
  test::MetricsConsentOverride metrics_consent(true);

  ASSERT_NE(GetPumaService(), nullptr);
  EXPECT_EQ(GetPumaService()->GetCountryIdHolderForTesting().GetForTesting(),
            country_codes::CountryId("BE"));
}

}  // namespace metrics::private_metrics
