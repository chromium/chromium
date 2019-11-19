// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

class SendTabToSelfModelMock : public TestSendTabToSelfModel {
 public:
  SendTabToSelfModelMock() = default;
  ~SendTabToSelfModelMock() override = default;

  bool IsReady() override { return true; }
  bool HasValidTargetDevice() override { return true; }
};

class TestSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService() = default;
  ~TestSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override {
    return &send_tab_to_self_model_mock_;
  }

 protected:
  SendTabToSelfModelMock send_tab_to_self_model_mock_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSendTabToSelfSyncService>();
}

class SendTabToSelfUtilTest : public BrowserWithTestWindowTest {
 public:
  SendTabToSelfUtilTest() = default;
  ~SendTabToSelfUtilTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    incognito_profile_ = profile()->GetOffTheRecordProfile();
    url_ = GURL("https://www.google.com");
    title_ = base::UTF8ToUTF16(base::StringPiece("Google"));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  Profile* incognito_profile_;
  GURL url_;
  base::string16 title_;
};

TEST_F(SendTabToSelfUtilTest, HasValidTargetDevice) {
  EXPECT_FALSE(HasValidTargetDevice(profile()));

  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));

  EXPECT_TRUE(HasValidTargetDevice(profile()));
}

TEST_F(SendTabToSelfUtilTest, ContentRequirementsMet) {
  EXPECT_TRUE(AreContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, NotHTTPOrHTTPS) {
  url_ = GURL("192.168.0.0");
  EXPECT_FALSE(AreContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, NativePage) {
  url_ = GURL("chrome://flags");
  EXPECT_FALSE(AreContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, IncognitoMode) {
  EXPECT_FALSE(AreContentRequirementsMet(url_, incognito_profile_));
}

TEST_F(SendTabToSelfUtilTest, ShouldNotOfferFeatureForTelephoneLink) {
  url_ = GURL("tel:07387252578");

  AddTab(browser(), url_);
  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));

  // get web contents
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(ShouldOfferFeatureForLink(web_contents, url_));
}

TEST_F(SendTabToSelfUtilTest, ShouldOfferFeatureForGoogleLink) {
  AddTab(browser(), url_);
  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));

  // get web contents
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(ShouldOfferFeatureForLink(web_contents, url_));
}

TEST_F(SendTabToSelfUtilTest, ShouldNotOfferFeatureInOmniboxWhileNavigating) {
  AddTab(browser(), url_);
  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(web_contents->IsWaitingForResponse());
  EXPECT_TRUE(ShouldOfferOmniboxIcon(web_contents));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("http://test.com/"), web_contents->GetMainFrame());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  EXPECT_TRUE(web_contents->IsWaitingForResponse());
  EXPECT_FALSE(ShouldOfferOmniboxIcon(web_contents));

  simulator->Commit();
  EXPECT_FALSE(web_contents->IsWaitingForResponse());
  EXPECT_TRUE(ShouldOfferOmniboxIcon(web_contents));
}

}  // namespace

}  // namespace send_tab_to_self
