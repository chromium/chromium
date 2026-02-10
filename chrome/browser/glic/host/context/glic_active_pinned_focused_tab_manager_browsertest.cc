// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_active_pinned_focused_tab_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
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

}  // namespace glic
