// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/merchant_trust_service_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "components/page_info/core/features.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

class MerchantTrustServiceDelegateTest : public ::testing::Test {
 public:
  void SetUp() override {
    delegate_ = std::make_unique<MerchantTrustServiceDelegate>(profile());
  }

  TestingProfile* profile() { return &profile_; }
  MerchantTrustServiceDelegate* delegate() { return delegate_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<MerchantTrustServiceDelegate> delegate_;
};

TEST_F(MerchantTrustServiceDelegateTest, GetSiteEngagementScore) {
  site_engagement::SiteEngagementService::Get(profile())->ResetBaseScoreForURL(
      GURL("https://highengagement.com"), 20);
  EXPECT_EQ(
      delegate()->GetSiteEngagementScore(GURL("https://highengagement.com")),
      20);
}
