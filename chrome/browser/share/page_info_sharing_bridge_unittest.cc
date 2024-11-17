// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/page_info_sharing_bridge.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

class PageInfoSharingBridgeTest : public testing::Test {
 public:
  PageInfoSharingBridgeTest() = default;
  ~PageInfoSharingBridgeTest() override = default;

  void SetUp() override {
    Test::SetUp();
    profile_ = TestingProfile::Builder().Build();
    identity_manager_ = identity_test_env_.identity_manager();
  }

  void TearDown() override {
    Test::TearDown();
    identity_manager_ = nullptr;
    profile_.reset();
  }

  void SetModelExecutionAvailable(bool available) {
    auto account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    ASSERT_FALSE(account_info.account_id.empty());
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(available);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  // Ensure RenderFrameHostTester to be created and used by the tests.
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<signin::IdentityManager> identity_manager_;
};

TEST_F(PageInfoSharingBridgeTest, ProfileWithoutSupport) {
  EXPECT_FALSE(sharing::DoesProfileSupportPageInfo(identity_manager_));

  SetModelExecutionAvailable(false);
  EXPECT_FALSE(sharing::DoesProfileSupportPageInfo(identity_manager_));
}

TEST_F(PageInfoSharingBridgeTest, ProfileWithSupport) {
  SetModelExecutionAvailable(true);
  EXPECT_TRUE(sharing::DoesProfileSupportPageInfo(identity_manager_));
}

TEST_F(PageInfoSharingBridgeTest, CheckLanguage) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), nullptr);
  EXPECT_FALSE(DoesWebContentsSupportPageInfo(web_contents.get()));

  // TODO(ssid): Set the page language to test working.
}

}  // namespace sharing
