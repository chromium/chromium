// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <memory>

#include "base/functional/bind.h"
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

// A fake that's always ready to offer send-tab-to-self.
class FakeSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  FakeSendTabToSelfSyncService() = default;
  ~FakeSendTabToSelfSyncService() override = default;

  absl::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
      const GURL&) override {
    return EntryPointDisplayReason::kOfferFeature;
  }
};

std::unique_ptr<KeyedService> BuildFakeSendTabToSelfSyncService(
    content::BrowserContext*) {
  return std::make_unique<FakeSendTabToSelfSyncService>();
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
};

TEST_F(SendTabToSelfUtilTest, ShouldHideEntryPointInOmniboxWhileNavigating) {
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
