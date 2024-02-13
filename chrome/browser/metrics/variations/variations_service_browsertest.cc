// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace variations {

class VariationsServiceBrowserTest : public InProcessBrowserTest {
 public:
  VariationsServiceBrowserTest() = default;

  VariationsServiceBrowserTest(const VariationsServiceBrowserTest&) = delete;
  VariationsServiceBrowserTest& operator=(const VariationsServiceBrowserTest&) =
      delete;

  PrefService* local_state() { return g_browser_process->local_state(); }
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(VariationsServiceBrowserTest,
                       LimitedEntropySyntheticTrialSeedTransfer) {
  auto* ash_params = chromeos::BrowserParamsProxy::Get();
  variations::VariationsService* variations_service =
      g_browser_process->GetMetricsServicesManager()->GetVariationsService();
  // Due to version skew, the Ash chrome version used in test might not have
  // this field yet.
  if (ash_params->LimitedEntropySyntheticTrialSeed() != 0u) {
    EXPECT_EQ(variations_service->limited_entropy_synthetic_trial_
                  .GetRandomizationSeed(local_state()),
              ash_params->LimitedEntropySyntheticTrialSeed());
  }
}
#endif

}  // namespace variations
