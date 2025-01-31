// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_change_service.h"

#include "base/command_line.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromePasswordChangeServiceTest : public testing::Test {
 public:
  ChromePasswordChangeServiceTest() = default;
  ~ChromePasswordChangeServiceTest() override = default;

  affiliations::MockAffiliationService& affiliation_service() {
    return mock_affiliation_service_;
  }
  MockOptimizationGuideKeyedService& mock_optimization_service() {
    return mock_optimization_service_;
  }

  password_manager::PasswordChangeServiceInterface* change_service() {
    return &change_service_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_{
      password_manager::features::kImprovedPasswordChangeService};
  testing::StrictMock<affiliations::MockAffiliationService>
      mock_affiliation_service_;
  testing::StrictMock<MockOptimizationGuideKeyedService>
      mock_optimization_service_;
  ChromePasswordChangeService change_service_{&mock_affiliation_service_,
                                              &mock_optimization_service_};
};

TEST_F(ChromePasswordChangeServiceTest, PasswordChangeSupported) {
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(testing::Return(GURL("https://test.com/password/")));
  EXPECT_CALL(mock_optimization_service(),
              ShouldFeatureAllowModelExecutionForSignedInUser)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(url));
}

TEST_F(ChromePasswordChangeServiceTest, PasswordChangeNotSupportedNoUrl) {
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(testing::Return(GURL()));
  EXPECT_CALL(mock_optimization_service(),
              ShouldFeatureAllowModelExecutionForSignedInUser)
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(change_service()->IsPasswordChangeSupported(url));
}

TEST_F(ChromePasswordChangeServiceTest,
       PasswordChangeNotSupportedSettingNotVisible) {
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(testing::Return(GURL("https://test.com/password/")));
  EXPECT_CALL(
      mock_optimization_service(),
      ShouldFeatureAllowModelExecutionForSignedInUser(
          optimization_guide::UserVisibleFeatureKey::kPasswordChangeSubmission))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(change_service()->IsPasswordChangeSupported(url));
}

TEST_F(ChromePasswordChangeServiceTest,
       PasswordChangeSupportedIfCommandLineArgProvided) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPasswordChangeUrl, "https://test.com/new_password/");

  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);

  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(url));
}

TEST_F(ChromePasswordChangeServiceTest,
       PasswordChangeSupportedIfPSLMatchedInArg) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPasswordChangeUrl, "https://test.com/new_password/");

  GURL url("https://www.test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);

  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(url));
}
