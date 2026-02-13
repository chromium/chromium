// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_active_pinned_focused_tab_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicActivePinnedFocusedTabManagerBrowserTest
    : public NonInteractiveGlicTest {
 public:
  GlicActivePinnedFocusedTabManagerBrowserTest() {
    // Enable multi-instance and multi-tab.
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicMultiInstance,
         mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines},
        {});
  }

 protected:
  // Setup tabs for test and return handles. Uses current tab, but if count > 1
  // then additional tabs will be created.
  std::vector<tabs::TabInterface*> SetupTabs(int count) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    for (int i = 0; i < count - 1; ++i) {
      EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
          browser(), GURL("about:blank"),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    }

    std::vector<tabs::TabInterface*> tabs;
    for (int i = 0; i < count; ++i) {
      tabs::TabInterface* tab = GetTabListInterface()->GetTab(i);
      EXPECT_TRUE(tab);
      if (tab) {
        tabs.push_back(tab);
      }
    }
    return tabs;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActivePinnedFocusedTabManagerBrowserTest,
                       TakesPinnedTabStatusIntoAccount) {
  // 1. Initial setup.
  browser_activator().SetMode(BrowserActivator::Mode::kManual);

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);
  auto& manager = service->sharing_manager();

  // 2. Open a tab.
  std::vector<tabs::TabInterface*> tabs = SetupTabs(1);
  ASSERT_FALSE(tabs.empty());
  tabs::TabInterface* tab = tabs[0];
  ASSERT_TRUE(tab);

  // 3. Toggle Glic to ensure we are in a mode that uses
  // GlicActivePinnedFocusedTabManager (Attached Mode).
  service->ToggleUI(browser(), /*prevent_close=*/false,
                    mojom::InvocationSource::kTopChromeButton);

  // 4. Ensure tab is NOT pinned (ToggleUI might auto-pin in some configs).
  manager.UnpinTabs({tab->GetHandle()}, GlicUnpinTrigger::kUnknown);
  EXPECT_FALSE(manager.IsTabPinned(tab->GetHandle()));

  auto focused_data = manager.GetFocusedTabData();
  // Expect no focus because the active tab is not pinned.
  EXPECT_FALSE(focused_data.focus());

  // 5. Pin the tab.
  manager.PinTabs({tab->GetHandle()}, GlicPinTrigger::kUnknown);
  EXPECT_TRUE(manager.IsTabPinned(tab->GetHandle()));

  // 6. Verify tab is now focused.
  auto focused_data_pinned = manager.GetFocusedTabData();
  ASSERT_TRUE(focused_data_pinned.focus());
  EXPECT_EQ(focused_data_pinned.focus()->GetHandle(), tab->GetHandle());

  // 7. Unpin the tab.
  manager.UnpinTabs({tab->GetHandle()}, GlicUnpinTrigger::kUnknown);
  EXPECT_FALSE(manager.IsTabPinned(tab->GetHandle()));

  // 8. Verify tab is no longer focused.
  auto focused_data_unpinned = manager.GetFocusedTabData();
  EXPECT_FALSE(focused_data_unpinned.focus());
}

IN_PROC_BROWSER_TEST_F(GlicActivePinnedFocusedTabManagerBrowserTest,
                       TakesActiveTabStatusIntoAccount) {
  browser_activator().SetMode(BrowserActivator::Mode::kManual);

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);
  auto& manager = service->sharing_manager();

  std::vector<tabs::TabInterface*> tabs = SetupTabs(2);
  ASSERT_EQ(tabs.size(), 2u);
  tabs::TabInterface* tab1 = tabs[0];
  tabs::TabInterface* tab2 = tabs[1];
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);
  ASSERT_NE(tab1, tab2);

  service->ToggleUI(browser(), /*prevent_close=*/false,
                    mojom::InvocationSource::kTopChromeButton);

  // Make sure both tabs are pinned.
  manager.PinTabs({tab1->GetHandle(), tab2->GetHandle()},
                  GlicPinTrigger::kUnknown);
  EXPECT_TRUE(manager.IsTabPinned(tab1->GetHandle()));
  EXPECT_TRUE(manager.IsTabPinned(tab2->GetHandle()));

  // Verify tab2 (active) is focused.
  auto focused_data = manager.GetFocusedTabData();
  ASSERT_TRUE(focused_data.focus());
  EXPECT_EQ(focused_data.focus()->GetHandle(), tab2->GetHandle());

  // Activate tab 1.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(browser()->GetActiveTabInterface(), tab1);

  // Verify tab 1 is focused.
  auto focused_data_final = manager.GetFocusedTabData();
  ASSERT_TRUE(focused_data_final.focus());
  EXPECT_EQ(focused_data_final.focus()->GetHandle(), tab1->GetHandle());
}

IN_PROC_BROWSER_TEST_F(GlicActivePinnedFocusedTabManagerBrowserTest,
                       DoesNotTriggerFocusChangeOnPinChangesToInactiveTabs) {
  browser_activator().SetMode(BrowserActivator::Mode::kManual);

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);
  auto& manager = service->sharing_manager();

  // Open two tabs.
  std::vector<tabs::TabInterface*> tabs = SetupTabs(2);
  ASSERT_EQ(tabs.size(), 2u);
  tabs::TabInterface* tab1 = tabs[0];
  tabs::TabInterface* tab2 = tabs[1];

  service->ToggleUI(browser(), /*prevent_close=*/false,
                    mojom::InvocationSource::kTopChromeButton);

  // Pin active tab (tab2).
  manager.PinTabs({tab2->GetHandle()}, GlicPinTrigger::kUnknown);
  EXPECT_TRUE(manager.IsTabPinned(tab2->GetHandle()));

  // Verify tab2 is focused.
  auto focused_data = manager.GetFocusedTabData();
  ASSERT_TRUE(focused_data.focus());
  EXPECT_EQ(focused_data.focus()->GetHandle(), tab2->GetHandle());

  // Monitor focus changes.
  base::test::TestFuture<void> future;
  auto subscription = manager.AddFocusedTabChangedCallback(
      base::IgnoreArgs<const FocusedTabData&>(future.GetRepeatingCallback()));

  // Pin the INACTIVE tab (tab1).
  manager.PinTabs({tab1->GetHandle()}, GlicPinTrigger::kUnknown);
  EXPECT_TRUE(manager.IsTabPinned(tab1->GetHandle()));

  // Verify focus did not change.
  EXPECT_FALSE(future.IsReady());

  // Verify focused tab is still tab2.
  auto focused_data_after = manager.GetFocusedTabData();
  ASSERT_TRUE(focused_data_after.focus());
  EXPECT_EQ(focused_data_after.focus()->GetHandle(), tab2->GetHandle());

  // Unpin the INACTIVE tab (tab1).
  manager.UnpinTabs({tab1->GetHandle()}, GlicUnpinTrigger::kUnknown);
  EXPECT_FALSE(manager.IsTabPinned(tab1->GetHandle()));

  // Verify focus did not change.
  EXPECT_FALSE(future.IsReady());
}

}  // namespace glic
