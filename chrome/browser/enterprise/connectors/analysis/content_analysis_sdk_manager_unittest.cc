// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"

#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class FakeClient : public content_analysis::sdk::Client {
 public:
  explicit FakeClient(const content_analysis::sdk::Client::Config& config)
      : config_(config) {}
  ~FakeClient() override = default;

  const Config& GetConfig() const override { return config_; }

  // Sends an analysis request to the agent and waits for a response.
  int Send(const content_analysis::sdk::ContentAnalysisRequest& request,
           content_analysis::sdk::ContentAnalysisResponse* response) override {
    return -1;
  }

  // Sends an response acknowledgment back to the agent.
  int Acknowledge(const content_analysis::sdk::ContentAnalysisAcknowledgement&
                      ack) override {
    return -1;
  }

 private:
  content_analysis::sdk::Client::Config config_;
};

// A derivative of ContentAnalysisSdkManager that creates fake SDK clients
// in order to not depend on having a real service provide agent running.
class FakeContentAnalysisSdkManager : public ContentAnalysisSdkManager {
  std::unique_ptr<content_analysis::sdk::Client> CreateClient(
      const content_analysis::sdk::Client::Config& config) override {
    return std::make_unique<FakeClient>(config);
  }
};

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
