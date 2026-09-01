// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_cleanup.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace policy::local_user_files {

class LocalFilesCleanupTest : public policy::PolicyTest {
 public:
  LocalFilesCleanupTest() {
    // Disable SkyVaultV2 - cleanup doesn't apply for GA.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kSkyVault},
        /*disabled_features=*/{ash::features::kSkyVaultV2});
  }
  ~LocalFilesCleanupTest() override = default;

 protected:
  void SetPolicyValue(bool local_user_files_allowed) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies,
                                  policy::key::kLocalUserFilesAllowed,
                                  base::Value(local_user_files_allowed));
    // The policy service publishes updates to the browser. Wait for that
    // publication so the policy observer starts cleanup before this test waits
    // for directory deletion. Without the service, UpdateChromePolicy uses an
    // internal RunUntilIdle() fallback, which previously let the observer and
    // cleanup work start. Clear the temporary connection after publication
    // because the provider stores only a raw pointer that must not survive into
    // browser shutdown.
    provider_.SetupPolicyServiceForPolicyUpdates(
        g_browser_process->policy_service());
    provider_.UpdateChromePolicy(policies);
    provider_.SetupPolicyServiceForPolicyUpdates(nullptr);
  }

  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalFilesCleanupTest, Cleanup) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath my_files_path =
      browser()->GetProfile()->GetPath().AppendASCII("MyFiles");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDirUnderPath(my_files_path));
  ASSERT_TRUE(base::DirectoryExists(temp_dir.GetPath()));

  // LocalFilesCleanup is already initialized in UserCloudPolicyManagerAsh
  // and will trigger cleanup.
  SetPolicyValue(false);

  ASSERT_TRUE(base::test::RunUntil([&temp_dir] {
    return !base::DirectoryExists(temp_dir.GetPath());
  })) << "Timed out waiting for local files cleanup";
  histogram_tester_.ExpectUniqueSample("SkyVault.LocalUserFilesCleanupCount",
                                       /*sample=*/1,
                                       /*expected_bucket_count=*/1);
}

}  // namespace policy::local_user_files
