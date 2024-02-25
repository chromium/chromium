// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"

#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_client.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"
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

TEST_F(ContentAnalysisSdkManagerTest, ResetClient) {
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

TEST_F(ContentAnalysisSdkManagerTest, ResetAllClients) {
  content_analysis::sdk::Client::Config config1{"local_test1"};
  content_analysis::sdk::Client::Config config2{"local_test2"};
  FakeContentAnalysisSdkManager manager;

  auto wrapped1 = manager.GetClient(config1);
  auto wrapped2 = manager.GetClient(config2);

  manager.ResetAllClients();
  EXPECT_TRUE(manager.NoConnectionEstablished());
  EXPECT_FALSE(manager.HasClientForTesting(config1));
  EXPECT_FALSE(manager.HasClientForTesting(config2));

  // Existing refptrs should still be valid.
  EXPECT_TRUE(wrapped1->HasOneRef());
  EXPECT_EQ(config1.name, wrapped1->client()->GetConfig().name);
  EXPECT_EQ(config1.user_specific,
            wrapped1->client()->GetConfig().user_specific);
  EXPECT_TRUE(wrapped2->HasOneRef());
  EXPECT_EQ(config2.name, wrapped2->client()->GetConfig().name);
  EXPECT_EQ(config2.user_specific,
            wrapped2->client()->GetConfig().user_specific);
}

}  // namespace enterprise_connectors
