// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

const char kHttpsUrl[] = "https://www.foo.com";
const char kHttpsUrl2[] = "https://www.bar.com";

class FakeSendTabToSelfModel : public TestSendTabToSelfModel {
 public:
  FakeSendTabToSelfModel() = default;
  ~FakeSendTabToSelfModel() override = default;

  void SetIsReady(bool is_ready) { is_ready_ = is_ready; }
  void SetHasValidTargetDevice(bool has_valid_target_device) {
    if (has_valid_target_device) {
      DCHECK(is_ready_) << "Target devices are only known if the model's ready";
    }
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
  return std::make_unique<FakeSendTabToSelfSyncService>();
}

std::unique_ptr<KeyedService> BuildTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

class SendTabToSelfUtilTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    AddTab(browser(), GURL("about:blank"));
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SendTabToSelfSyncServiceFactory::GetInstance(),
             base::BindRepeating(&BuildFakeSendTabToSelfSyncService)},
            {SyncServiceFactory::GetInstance(),
             base::BindRepeating(&BuildTestSyncService)}};
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  FakeSendTabToSelfSyncService* service() {
    return static_cast<FakeSendTabToSelfSyncService*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  }

  void SignIn() {
    CoreAccountInfo account;
    account.gaia = "gaia_id";
    account.email = "email@test.com";
    account.account_id = CoreAccountId::FromGaiaId(account.gaia);
    static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()))
        ->SetAccountInfo(account);
  }
};

TEST_F(SendTabToSelfUtilTest, ShouldHideEntryPointInOmniboxWhileNavigating) {
  SignIn();
  service()->GetSendTabToSelfModel()->SetIsReady(true);
  service()->GetSendTabToSelfModel()->SetHasValidTargetDevice(true);
  NavigateAndCommitActiveTab(GURL(kHttpsUrl));

  ASSERT_FALSE(web_contents()->IsWaitingForResponse());
  EXPECT_TRUE(ShouldOfferOmniboxIcon(web_contents()));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kHttpsUrl2), web_contents()->GetPrimaryMainFrame());
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
