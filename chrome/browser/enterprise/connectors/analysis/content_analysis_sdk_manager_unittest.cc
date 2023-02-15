// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"

#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_client.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_connectors {

class ContentAnalysisSdkManagerTest : public testing::Test {
 public:
  ContentAnalysisSdkManagerTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ContentAnalysisSdkManagerTest, Create) {
  content_analysis::sdk::Client::Config config{"local_test"};
  FakeContentAnalysisSdkManager manager;

  EXPECT_FALSE(manager.HasClientForTesting(config));

  auto wrapped = manager.GetClient(config);
  EXPECT_TRUE(manager.HasClientForTesting(config));
  EXPECT_TRUE(wrapped->HasAtLeastOneRef());
}

TEST_F(ContentAnalysisSdkManagerTest, Reset) {
  content_analysis::sdk::Client::Config config{"local_test"};
  FakeContentAnalysisSdkManager manager;

  auto wrapped = manager.GetClient(config);

  manager.ResetClient(config);
  EXPECT_FALSE(manager.HasClientForTesting(config));

  // Existing refptr should still be valid.
  EXPECT_TRUE(wrapped->HasOneRef());
  EXPECT_EQ(config.name, wrapped->client()->GetConfig().name);
  EXPECT_EQ(config.user_specific, wrapped->client()->GetConfig().user_specific);
}

#if BUILDFLAG(IS_WIN)
// This tests a tempporary change needed only for windows until m115.
// When the change is removed, this test can also be removed.
TEST_F(ContentAnalysisSdkManagerTest, UseBrcmChrmCasIfNoPathSystem) {
  content_analysis::sdk::Client::Config config{"path_system"};
  ContentAnalysisSdkManager* manager = ContentAnalysisSdkManager::Get();

  // When no agent is running, the SDK managed cannot get a client.
  auto wrapped = manager->GetClient(config);
  EXPECT_FALSE(wrapped);
  EXPECT_FALSE(manager->HasClientForTesting(config));

  // Start an agent using the "brcm_chrm_cas" pipe name.

  base::FilePath out_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
  base::FilePath exe_path = out_dir.Append(FILE_PATH_LITERAL("lca_agent.exe"));

  base::CommandLine cmdline(exe_path);
  base::LaunchOptions options;
  base::Process process = base::LaunchProcess(cmdline, options);
  ASSERT_TRUE(process.IsValid());

  // Now the SDK manager should be able to get a client.  May need to try
  // a few times to give the agent a chance to start listening for
  // connections.
  for (int i = 0; i < 5; ++i) {
    wrapped = manager->GetClient(config);
    if (wrapped) {
      break;
    }

    // Sleep for one second.
    base::PlatformThread::Sleep(base::Seconds(1));
  }

  EXPECT_TRUE(wrapped);
  EXPECT_TRUE(manager->HasClientForTesting(config));
  EXPECT_TRUE(process.Terminate(0, false));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace enterprise_connectors
