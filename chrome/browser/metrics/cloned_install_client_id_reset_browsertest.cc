// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/json_pref_store.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace {
const char kInitialClientId[] = "11111111-2222-aaaa-bbbb-cccccccccccc";
}  // namespace

class ClonedInstallClientIdResetBrowserTest : public InProcessBrowserTest {
 public:
  ClonedInstallClientIdResetBrowserTest() = default;
  ~ClonedInstallClientIdResetBrowserTest() override = default;

  bool SetUpUserDataDirectory() override {
    if (!InProcessBrowserTest::SetUpUserDataDirectory())
      return false;

    // Changing in user's data directory should only be done once before
    // PRE_TestClonedInstallClientIdReset.
    if (!content::IsPreTest())
      return true;

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath user_dir;
    CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_dir));

    // Create a local-state file with what we want the browser to use. This
    // has to be done here because there is no hook between when the browser
    // is initialized and the metrics-client acts on the pref values. The
    // "Local State" directory is hard-coded because the FILE_LOCAL_STATE
    // path is not yet defined at this point.
    base::test::TaskEnvironment task_env;
    auto state = base::MakeRefCounted<JsonPrefStore>(
        user_dir.Append(FILE_PATH_LITERAL("Local State")));

    // Set up the initial client id for (before)
    // PRE_TestClonedInstallClientIdReset.
    state->SetValue(metrics::prefs::kMetricsClientID,
                    base::Value(kInitialClientId), 0);

    return true;
  }

  void SetUp() override {
    // Make metrics reporting work same as in Chrome branded builds, for test
    // consistency between Chromium and Chrome builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_enabled_);

    InProcessBrowserTest::SetUp();
  }

  metrics::MetricsService* metrics_service() {
    return g_browser_process->GetMetricsServicesManager()->GetMetricsService();
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

 private:
  bool metrics_enabled_ = true;
};

IN_PROC_BROWSER_TEST_F(ClonedInstallClientIdResetBrowserTest,
                       PRE_TestClonedInstallClientIdReset) {
  local_state()->SetBoolean(metrics::prefs::kMetricsResetIds, true);
  EXPECT_EQ(kInitialClientId, metrics_service()->GetClientId());
}

IN_PROC_BROWSER_TEST_F(ClonedInstallClientIdResetBrowserTest,
                       TestClonedInstallClientIdReset) {
  EXPECT_NE(kInitialClientId, metrics_service()->GetClientId());
  EXPECT_FALSE(local_state()->GetBoolean(metrics::prefs::kMetricsResetIds));
}
