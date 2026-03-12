// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics_provider.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicMetricsProviderTest : public testing::Test {
 public:
  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
  }

  void TearDown() override { testing_profile_manager_.reset(); }

 protected:
  TestingProfileManager* profile_manager() {
    return testing_profile_manager_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
};

TEST_F(GlicMetricsProviderTest, ProvideCurrentSessionData) {
  profile_manager()->CreateTestingProfile("profile1");
  profile_manager()->CreateTestingProfile("profile2");

  GlicMetricsProvider provider;
  base::HistogramTester histograms;

  provider.ProvideCurrentSessionData(nullptr);

  // Should have recorded metrics for both profiles.
  histograms.ExpectTotalCount("Glic.ProfileEnablement.IsEnabled.SteadyState",
                              2);
}

}  // namespace glic
