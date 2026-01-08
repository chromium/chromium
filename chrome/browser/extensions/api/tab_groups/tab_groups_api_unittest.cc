// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router_factory.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/data_sharing/public/features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildTabGroupsEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<TabGroupsEventRouter>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::EventRouter>(
      context, ExtensionPrefs::Get(context));
}

}  // namespace

class TabGroupsApiUnitTest : public ExtensionServiceTestBase {
 public:
  TabGroupsApiUnitTest() = default;
  TabGroupsApiUnitTest(const TabGroupsApiUnitTest&) = delete;
  TabGroupsApiUnitTest& operator=(const TabGroupsApiUnitTest&) = delete;
  ~TabGroupsApiUnitTest() override = default;

 protected:
  Browser* browser() { return browser_.get(); }
  TestBrowserWindow* browser_window() { return browser_window_.get(); }

  content::WebContents* web_contents(int index) {
    return web_contentses_[index];
  }

  void WaitForTabGroupSyncServiceInitialized() {
    auto observer =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()));
    observer->Wait();
  }

  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // The browser (and accompanying window).
  raw_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  // The original web contentses in order.
  std::vector<raw_ptr<content::WebContents, VectorExperimental>>
      web_contentses_;
};

void TabGroupsApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  // Create a browser window.
  auto browser_window = std::make_unique<TestBrowserWindow>();
  browser_window_ = browser_window.get();
  Browser::CreateParams params(profile(), /* user_gesture */ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window.release();
  browser_ = Browser::DeprecatedCreateOwnedForTesting(params);
  BrowserList::SetLastActive(browser_.get());

  // Add several tabs to the browser and get their tab IDs and web contents.
  constexpr int kNumTabs = 6;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    web_contentses_.push_back(contents.get());
    browser_->tab_strip_model()->AppendWebContents(std::move(contents),
                                                   /* foreground */ true);
  }

  TabGroupsEventRouterFactory::GetInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&BuildTabGroupsEventRouter));

  EventRouterFactory::GetInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&BuildEventRouter));

  // We need to call TabGroupsEventRouterFactory::Get() in order to instantiate
  // the keyed service, since it's not created by default in unit tests.
  TabGroupsEventRouterFactory::Get(browser_context());

  // Wait for the TabGroupSyncService to properly initialize before making any
  // changes to tab groups.
  WaitForTabGroupSyncServiceInitialized();
}

void TabGroupsApiUnitTest::TearDown() {
  browser_window_ = nullptr;
  browser_->tab_strip_model()->CloseAllTabs();
  browser_.reset();
  ExtensionServiceTestBase::TearDown();
}

TEST_F(TabGroupsApiUnitTest, TabGroupsOnCreated) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  browser()->tab_strip_model()->AddToNewGroup({1, 2, 3});

  EXPECT_EQ(2u, event_observer.events().size());
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnCreated::kEventName));
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnUpdated::kEventName));
}

TEST_F(TabGroupsApiUnitTest, TabGroupsOnUpdated) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({1, 2, 3});

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  tab_groups::TabGroupVisualData visual_data(u"Title",
                                             tab_groups::TabGroupColorId::kRed);
  tab_strip_model->ChangeTabGroupVisuals(group, visual_data);

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnUpdated::kEventName));
}

TEST_F(TabGroupsApiUnitTest, TabGroupsOnRemoved) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddToNewGroup({1, 2, 3});

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  tab_strip_model->RemoveFromGroup({1, 2, 3});

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnRemoved::kEventName));
}

TEST_F(TabGroupsApiUnitTest, TabGroupsOnMoved) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({1, 2, 3});

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  tab_strip_model->MoveGroupTo(group, 0);

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnMoved::kEventName));
}

}  // namespace extensions
