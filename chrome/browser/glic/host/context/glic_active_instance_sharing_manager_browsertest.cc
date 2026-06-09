// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/test/run_until.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicActiveInstanceSharingManagerBrowserTest
    : public NonInteractiveGlicTest {
 public:
  GlicActiveInstanceSharingManagerBrowserTest() {
    // Enable multi-instance and multi-tab to ensure
    // GlicActiveInstanceSharingManager is used.
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicMultiInstance,
         mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// (crbug.com/479963426): Test is highly flakey on win-rel cq builder.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(GlicActiveInstanceSharingManagerBrowserTest,
                       DelegatesToActiveInstance) {
  // 1. Initial state: no instance, so no delegate.
  // GlicActiveInstanceSharingManager delegates to nothing if no active
  // instance. We can verify this by checking if it seems empty.
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);
  auto& manager = service->active_instance_sharing_manager();
  EXPECT_TRUE(manager.GetPinnedTabs().empty());

  // 2. Open a tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 3. Toggle Glic to create an instance.
  service->ToggleUI(browser(), false,
                    mojom::InvocationSource::kTopChromeButton);

  auto* instance = service->GetInstanceForActiveTab(browser());
  ASSERT_TRUE(instance);

  // 4. Pin a tab on the instance's sharing manager.
  auto& instance_sharing_manager = instance->host().sharing_manager();

  // Get a tab handle.
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  instance_sharing_manager.PinTabs({tab->GetHandle()},
                                   GlicPinTrigger::kUnknown);

  // 5. Verify the main sharing manager sees it (delegation working).
  EXPECT_TRUE(manager.IsTabPinned(tab->GetHandle()));

  // 6. Verify another browser window doesn't see it (delegation follows active
  // window). Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  // Helper to activate.
  browser2->GetWindow()->Activate();

  // Now `active_instance` for the sharing manager should be null (or whatever
  // is on browser2, which is nothing yet). Note:
  // GlicActiveInstanceSharingManager updates its delegate based on the active
  // instance. The active instance is determined by the active browser's active
  // tab's instance. Since browser2 has no instance, delegate should be null.

  EXPECT_FALSE(manager.IsTabPinned(tab->GetHandle()));

  // Open Glic on browser2.
  service->ToggleUI(browser2, false, mojom::InvocationSource::kTopChromeButton);
  auto* instance2 = service->GetInstanceForActiveTab(browser2);
  ASSERT_TRUE(instance2);
  ASSERT_NE(instance, instance2);

  // Pin a NEW tab on instance2.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GURL("about:blank")));
  tabs::TabInterface* tab2 = browser2->GetActiveTabInterface();
  ASSERT_TRUE(tab2);
  // Ensure tab2 is different from tab1 (should be guaranteed by different
  // browsers).
  ASSERT_NE(tab->GetHandle(), tab2->GetHandle());

  auto& instance2_sharing_manager = instance2->host().sharing_manager();
  instance2_sharing_manager.PinTabs({tab2->GetHandle()},
                                    GlicPinTrigger::kUnknown);

  // Verify delegation to instance2: tab2 pinned, tab1 NOT pinned.
  // Use RunUntil to handle potential window activation delays on Linux.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return service->active_instance_sharing_manager().IsTabPinned(
               tab2->GetHandle()) &&
           !service->active_instance_sharing_manager().IsTabPinned(
               tab->GetHandle());
  }));

  // Switch back to browser1.
  browser()->GetWindow()->Activate();

  // Verify delegation to instance1: tab1 pinned, tab2 NOT pinned.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return service->active_instance_sharing_manager().IsTabPinned(
               tab->GetHandle()) &&
           !service->active_instance_sharing_manager().IsTabPinned(
               tab2->GetHandle());
  }));
}
#endif

class GlicActiveInstanceSharingManagerProfileStateTest
    : public NonInteractiveGlicTest {
 public:
  GlicActiveInstanceSharingManagerProfileStateTest() {
    scoped_feature_list_.InitWithFeatures({features::kGlic}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActiveInstanceSharingManagerProfileStateTest,
                       RespectsProfileState) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);

  // 1. Start with revoked consent.
  SetFRECompletion(browser()->profile(), prefs::FreStatus::kIncomplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  tabs::TabInterface* tab = TabListInterface::From(browser())->GetActiveTab();
  ASSERT_TRUE(tab);

  // 2. Toggle UI.
  service->ToggleUI(browser(), false,
                    mojom::InvocationSource::kTopChromeButton);

  auto& manager = service->active_instance_sharing_manager();

  // Instance should be created (showing FRE).
  auto* instance = service->GetInstanceForActiveTab(browser());
  ASSERT_TRUE(instance);

  instance->host().sharing_manager().PinTabs({tab->GetHandle()},
                                             GlicPinTrigger::kUnknown);

  // Verify delegation is OFF (manager doesn't see it).
  EXPECT_FALSE(manager.IsTabPinned(tab->GetHandle()));

  // Grant consent.
  SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);

  // Verify delegation resumes (dynamic update).
  EXPECT_TRUE(manager.IsTabPinned(tab->GetHandle()));
}

}  // namespace glic
