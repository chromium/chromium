// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/test/browser_test.h"

#if defined(OS_WIN)
#include "base/test/test_reg_util_win.h"
#endif  // defined(OS_WIN)

namespace {
const char kInitialClientId[] = "11111111-2222-aaaa-bbbb-cccccccccccc";
}  // namespace

class ClonedInstallClientIdResetBrowserTest : public InProcessBrowserTest {
 public:
  ClonedInstallClientIdResetBrowserTest() = default;
  ~ClonedInstallClientIdResetBrowserTest() override = default;

  void SetUp() override {
    // Make metrics reporting work same as in Chrome branded builds, for test
    // consistency between Chromium and Chrome builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_enabled_);

// On windows, the registry is used for client info backups.
#if defined(OS_WIN)
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
#endif  // defined(OS_WIN)

    metrics::ClientInfo client_info;
    client_info.client_id = kInitialClientId;
    GoogleUpdateSettings::StoreMetricsClientInfo(client_info);

    InProcessBrowserTest::SetUp();
  }

  metrics::MetricsService* metrics_service() {
    return g_browser_process->GetMetricsServicesManager()->GetMetricsService();
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

 private:
  bool metrics_enabled_ = true;

#if defined(OS_WIN)
  registry_util::RegistryOverrideManager registry_override_;
#endif  // defined(OS_WIN)
};

IN_PROC_BROWSER_TEST_F(ClonedInstallClientIdResetBrowserTest,
                       PRE_TestClonedInstallClientIdReset) {
  local_state()->SetBoolean(metrics::prefs::kMetricsResetIds, true);
  EXPECT_EQ(kInitialClientId, metrics_service()->GetClientId());
}

// Test is flaky on Mac (https://crbug.com/1175077).
#if defined(OS_MAC)
#define MAYBE_TestClonedInstallClientIdReset \
  DISABLED_TestClonedInstallClientIdReset
#else
#define MAYBE_TestClonedInstallClientIdReset TestClonedInstallClientIdReset
#endif
IN_PROC_BROWSER_TEST_F(ClonedInstallClientIdResetBrowserTest,
                       MAYBE_TestClonedInstallClientIdReset) {
  EXPECT_NE(kInitialClientId, metrics_service()->GetClientId());
  EXPECT_FALSE(local_state()->GetBoolean(metrics::prefs::kMetricsResetIds));
}
