// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"

#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_client.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace enterprise_connectors
