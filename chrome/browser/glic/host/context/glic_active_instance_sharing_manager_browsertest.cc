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
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicActiveInstanceSharingManagerBrowserTest : public GlicBrowserTest {
 public:
  GlicSharingManagerInternal& active_instance_sharing_manager() {
    return service()->active_instance_sharing_manager();
  }

  [[nodiscard]] TestResult<> WaitForPinnedTabs(
      const std::vector<tabs::TabInterface*>& expected_tabs) {
    return RunUntilEqual(
        [&]() { return active_instance_sharing_manager().GetPinnedTabs(); },
        expected_tabs);
  }
};

IN_PROC_BROWSER_TEST_F(GlicActiveInstanceSharingManagerBrowserTest,
                       DelegatesToActiveInstance) {
  // 1. Initial state: no instance, so no delegate.
  // GlicActiveInstanceSharingManager delegates to nothing if no active
  // instance. We can verify this by checking if it seems empty.
  auto& manager = active_instance_sharing_manager();
  EXPECT_TRUE(manager.GetPinnedTabs().empty());

  // 2. Open a tab.
  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));

  // 3. Toggle Glic to create an instance.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // 4. Pin a tab on the instance's sharing manager.
  auto* instance_sharing_manager = instance->GetSharingManager();

  instance_sharing_manager->PinTabs({tab->GetHandle()},
                                    GlicPinTrigger::kUnknown);

  // 5. Verify the main sharing manager sees it (delegation working).
  EXPECT_TRUE(manager.IsTabPinned(tab->GetHandle()));

  // 6. Verify another browser window doesn't see it (delegation follows active
  // window). Create another browser.
  BrowserWindowInterface* browser2 = CreateAdditionalBrowserWindow();
  browser2->GetWindow()->Activate();

  // Now `active_instance` for the sharing manager should be null (or whatever
  // is on browser2, which is nothing yet). Note:
  // GlicActiveInstanceSharingManager updates its delegate based on the active
  // instance. The active instance is determined by the active browser's active
  // tab's instance. Since browser2 has no instance, delegate should be null.

  EXPECT_FALSE(manager.IsTabPinned(tab->GetHandle()));

  // Pin a NEW tab on browser2.
  tabs::TabInterface* tab2 =
      CreateAndActivateTab(browser2, GURL("about:blank"));
  // Ensure tab2 is different from tab1 (should be guaranteed by different
  // browsers).
  ASSERT_NE(tab->GetHandle(), tab2->GetHandle());

  // Open Glic on browser2.
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForTab(tab2));
  ASSERT_NE(instance, instance2);

  auto* instance2_sharing_manager = instance2->GetSharingManager();
  instance2_sharing_manager->PinTabs({tab2->GetHandle()},
                                     GlicPinTrigger::kUnknown);

  // Verify delegation to instance2: tab2 pinned, tab1 NOT pinned.
  // Use RunUntil to handle potential window activation delays on Linux.
  ASSERT_OK(WaitForPinnedTabs({tab2}));

  // Switch back to browser1.
  GetBrowser()->GetWindow()->Activate();

  // Verify delegation to instance1: tab1 pinned, tab2 NOT pinned.
  ASSERT_OK(WaitForPinnedTabs({tab}));

  // Clean up browser2.
  browser2->GetWindow()->Close();
}

IN_PROC_BROWSER_TEST_F(GlicActiveInstanceSharingManagerBrowserTest,
                       RespectsProfileState) {
  // 1. Start with revoked consent.
  SetFRECompletion(GetProfile(), prefs::FreStatus::kIncomplete);
  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));

  // 2. Toggle UI.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  instance->GetSharingManager()->PinTabs({tab->GetHandle()},
                                         GlicPinTrigger::kUnknown);

  // Verify delegation is OFF (manager doesn't see it).
  ASSERT_OK(WaitForPinnedTabs({}));

  // Grant consent.
  SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);

  // Verify delegation resumes (dynamic update).
  ASSERT_OK(WaitForPinnedTabs({tab}));
}

}  // namespace glic
