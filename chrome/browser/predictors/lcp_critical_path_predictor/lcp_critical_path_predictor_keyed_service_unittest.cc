// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_keyed_service.h"

#include "base/task/thread_pool.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class LCPCriticalPathPredictorKeyedServiceTest : public testing::Test {
 public:
  ~LCPCriticalPathPredictorKeyedServiceTest() override;
  void SetUp() override;
  void TearDown() override {
    // Wait until the KeyedService finishes closing its database asynchronously,
    // so as not to leak after the test concludes.
    task_environment_.RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<LCPCriticalPathPredictorKeyedService>
      lcp_critical_path_predictor_;
};

LCPCriticalPathPredictorKeyedServiceTest::
    ~LCPCriticalPathPredictorKeyedServiceTest() = default;

void LCPCriticalPathPredictorKeyedServiceTest::SetUp() {
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  profile_ = std::make_unique<TestingProfile>();
  lcp_critical_path_predictor_ =
      std::make_unique<LCPCriticalPathPredictorKeyedService>(
          profile_.get(), std::move(db_task_runner));
  // Allow database initialization to complete.
  task_environment_.RunUntilIdle();
}

TEST_F(LCPCriticalPathPredictorKeyedServiceTest, SetAndThenGetLCPElementUrl) {
  // Basic test to set LCP Element and get its URL.

  const char kMainPageUrl[] = "http://example.com/";
  const char kLcpElementUrl[] = "http://example.com/lcp.png";

  GURL main_page(kMainPageUrl);

  // Set LCP element.
  LCPElement lcp_element;
  lcp_element.set_lcp_element_url(kLcpElementUrl);
  lcp_critical_path_predictor_->SetLCPElement(main_page, lcp_element);

  // Get LCP element's URL.
  absl::optional<LCPElement> found =
      lcp_critical_path_predictor_->GetLCPElement(main_page);
  ASSERT_TRUE(found);
  EXPECT_EQ(found->lcp_element_url(), kLcpElementUrl);
}
