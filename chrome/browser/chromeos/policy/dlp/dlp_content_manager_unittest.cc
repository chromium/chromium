// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot);
const DlpContentRestrictionSet kNonEmptyRestrictionSet = kScreenshotRestricted;
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen);
const DlpContentRestrictionSet kPrintingRestricted(
    DlpContentRestriction::kPrint);

class MockPrivacyScreenHelper : public ash::PrivacyScreenDlpHelper {
 public:
  MOCK_METHOD1(SetEnforced, void(bool));
};

}  // namespace

class DlpContentManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();

    profile_ = std::make_unique<TestingProfile>();
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

  void ChangeConfidentiality(content::WebContents* web_contents,
                             DlpContentRestrictionSet restrictions) {
    manager_.OnConfidentialityChanged(web_contents, restrictions);
  }

  void ChangeVisibility(content::WebContents* web_contents, bool visible) {
    if (visible) {
      web_contents->WasShown();
    } else {
      web_contents->WasHidden();
    }
    manager_.OnVisibilityChanged(web_contents);
  }

  void DestroyWebContents(content::WebContents* web_contents) {
    manager_.OnWebContentsDestroyed(web_contents);
  }

  base::TimeDelta GetPrivacyScreenOffDelay() {
    return manager_.GetPrivacyScreenOffDelayForTesting();
  }

  DlpContentManager manager_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DlpContentManagerTest, NoConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest, ConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  ChangeConfidentiality(web_contents.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kNonEmptyRestrictionSet);

  DestroyWebContents(web_contents.get());
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest, ConfidentialDataVisibilityChanged) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  ChangeConfidentiality(web_contents.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kNonEmptyRestrictionSet);

  web_contents->WasHidden();
  ChangeVisibility(web_contents.get(), /*visible=*/false);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  web_contents->WasShown();
  ChangeVisibility(web_contents.get(), /*visible=*/true);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kNonEmptyRestrictionSet);

  DestroyWebContents(web_contents.get());
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest,
       TwoWebContentsVisibilityAndConfidentialityChanged) {
  std::unique_ptr<content::WebContents> web_contents1 = CreateWebContents();
  std::unique_ptr<content::WebContents> web_contents2 = CreateWebContents();
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  // WebContents 1 becomes confidential.
  ChangeConfidentiality(web_contents1.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents1.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kNonEmptyRestrictionSet);

  // WebContents 2 is hidden.
  ChangeVisibility(web_contents2.get(), /*visible=*/false);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents1.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kNonEmptyRestrictionSet);

  // WebContents 1 becomes non-confidential.
  ChangeConfidentiality(web_contents1.get(), kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  // WebContents 2 becomes confidential.
  ChangeConfidentiality(web_contents2.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents2.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  // WebContents 2 is visible.
  ChangeVisibility(web_contents2.get(), /*visible=*/true);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents2.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kNonEmptyRestrictionSet);

  DestroyWebContents(web_contents1.get());
  DestroyWebContents(web_contents2.get());
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_.GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest, PrivacyScreenEnforcement) {
  MockPrivacyScreenHelper mock_privacy_screen_helper;
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(true)).Times(1);
  ChangeConfidentiality(web_contents.get(), kPrivacyScreenEnforced);

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(false)).Times(1);
  web_contents->WasHidden();
  ChangeVisibility(web_contents.get(), /*visible=*/false);
  task_environment_.FastForwardBy(GetPrivacyScreenOffDelay());

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(true)).Times(1);
  web_contents->WasShown();
  ChangeVisibility(web_contents.get(), /*visible=*/true);

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(false)).Times(1);
  DestroyWebContents(web_contents.get());
  task_environment_.FastForwardBy(GetPrivacyScreenOffDelay());
}

TEST_F(DlpContentManagerTest, PrintingRestricted) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(manager_.IsPrintingRestricted(web_contents.get()));

  ChangeConfidentiality(web_contents.get(), kPrintingRestricted);
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kPrintingRestricted);
  EXPECT_TRUE(manager_.IsPrintingRestricted(web_contents.get()));

  DestroyWebContents(web_contents.get());
  EXPECT_EQ(manager_.GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(manager_.IsPrintingRestricted(web_contents.get()));
}
}  // namespace policy
