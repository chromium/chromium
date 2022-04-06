// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

const char kHttpsUrl[] = "https://www.foo.com";
const char kHttpsUrl2[] = "https://www.bar.com";
const char kHttpUrl[] = "http://www.foo.com";

class FakeSendTabToSelfModel : public TestSendTabToSelfModel {
 public:
  FakeSendTabToSelfModel() = default;
  ~FakeSendTabToSelfModel() override = default;

  void SetIsReady(bool is_ready) { is_ready_ = is_ready; }
  void SetHasValidTargetDevice(bool has_valid_target_device) {
    has_valid_target_device_ = has_valid_target_device;
  }

  bool IsReady() override { return is_ready_; }
  bool HasValidTargetDevice() override { return has_valid_target_device_; }

 private:
  bool is_ready_ = false;
  bool has_valid_target_device_ = false;
};

class FakeSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  FakeSendTabToSelfSyncService() = default;
  ~FakeSendTabToSelfSyncService() override = default;

  FakeSendTabToSelfModel* GetSendTabToSelfModel() override { return &model_; }

 private:
  FakeSendTabToSelfModel model_;
};

std::unique_ptr<KeyedService> BuildFakeSendTabToSelfSyncService(
    content::BrowserContext*) {
  auto service = std::make_unique<FakeSendTabToSelfSyncService>();
  service->GetSendTabToSelfModel()->SetIsReady(true);
  service->GetSendTabToSelfModel()->SetHasValidTargetDevice(true);
  return service;
}

class SendTabToSelfUtilTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    AddTab(browser(), GURL("about:blank"));
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SendTabToSelfSyncServiceFactory::GetInstance(),
             base::BindRepeating(&BuildFakeSendTabToSelfSyncService)}};
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  FakeSendTabToSelfSyncService* service() {
    return static_cast<FakeSendTabToSelfSyncService*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  }
};

TEST_F(SendTabToSelfUtilTest, ShouldNotOfferFeatureIfModelNotReady) {
  service()->GetSendTabToSelfModel()->SetIsReady(false);
  service()->GetSendTabToSelfModel()->SetHasValidTargetDevice(true);

  NavigateAndCommitActiveTab(GURL(kHttpsUrl));
  EXPECT_FALSE(ShouldOfferFeature(web_contents()));
}

TEST_F(SendTabToSelfUtilTest, ShouldNotOfferFeatureIfHasNoValidTargetDevice) {
  service()->GetSendTabToSelfModel()->SetIsReady(true);
  service()->GetSendTabToSelfModel()->SetHasValidTargetDevice(false);

  NavigateAndCommitActiveTab(GURL(kHttpsUrl));
  EXPECT_FALSE(ShouldOfferFeature(web_contents()));
}

TEST_F(SendTabToSelfUtilTest, ShouldOnlyOfferFeatureIfHttpOrHttps) {
  service()->GetSendTabToSelfModel()->SetIsReady(true);
  service()->GetSendTabToSelfModel()->SetHasValidTargetDevice(true);

  NavigateAndCommitActiveTab(GURL(kHttpsUrl));
  EXPECT_TRUE(ShouldOfferFeature(web_contents()));

  NavigateAndCommitActiveTab(GURL(kHttpUrl));
  EXPECT_TRUE(ShouldOfferFeature(web_contents()));

  NavigateAndCommitActiveTab(GURL("192.168.0.0"));
  EXPECT_FALSE(ShouldOfferFeature(web_contents()));

  NavigateAndCommitActiveTab(GURL("chrome-untrusted://url"));
  EXPECT_FALSE(ShouldOfferFeature(web_contents()));

  NavigateAndCommitActiveTab(GURL("chrome://flags"));
  EXPECT_FALSE(ShouldOfferFeature(web_contents()));

  NavigateAndCommitActiveTab(GURL("tel:07399999999"));
  EXPECT_FALSE(ShouldOfferFeature(web_contents()));
}

TEST_F(SendTabToSelfUtilTest, ShouldNotOfferFeatureInIncognitoMode) {
  // TODO(crbug.com/1313539): This isn't a great way to fake an off-the-record
  // profile, but BrowserWithTestWindowTest lacks support. More concretely, this
  // harness relies on TestingProfileManager, and the only fitting method there
  // is broken (CreateGuestProfile()).
  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return nullptr;
          }));

  // Note: if changing this, audit profile-finding logic in the feature.
  // For example, NotificationManager.java in the Android code assumes
  // incognito is not supported.
  EXPECT_FALSE(ShouldOfferFeature(web_contents()));
}

TEST_F(SendTabToSelfUtilTest, ShouldNotOfferFeatureInOmniboxWhileNavigating) {
  NavigateAndCommitActiveTab(GURL(kHttpsUrl));

  ASSERT_FALSE(web_contents()->IsWaitingForResponse());
  EXPECT_TRUE(ShouldOfferOmniboxIcon(web_contents()));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kHttpsUrl2), web_contents()->GetMainFrame());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  ASSERT_TRUE(web_contents()->IsWaitingForResponse());
  EXPECT_FALSE(ShouldOfferOmniboxIcon(web_contents()));

  simulator->Commit();
  ASSERT_FALSE(web_contents()->IsWaitingForResponse());
  EXPECT_TRUE(ShouldOfferOmniboxIcon(web_contents()));
}

}  // namespace

}  // namespace send_tab_to_self
