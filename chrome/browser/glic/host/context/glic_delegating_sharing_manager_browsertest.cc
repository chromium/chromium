// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"

#include "base/test/test_future.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class GlicDelegatingSharingManagerBrowserTest : public NonInteractiveGlicTest {
 public:
  GlicDelegatingSharingManagerBrowserTest() = default;
  ~GlicDelegatingSharingManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    NonInteractiveGlicTest::SetUpOnMainThread();
    // No delegate initially.
  }

 protected:
  GlicDelegatingSharingManager manager_;
};

IN_PROC_BROWSER_TEST_F(GlicDelegatingSharingManagerBrowserTest,
                       EmptyDelegateBehavior) {
  // Verify GetFocusedTabData returns error/no focus
  FocusedTabData focused_tab_data = manager_.GetFocusedTabData();
  EXPECT_FALSE(focused_tab_data.is_focus());
  EXPECT_FALSE(focused_tab_data.GetFocus().has_value());

  // Verify PinTabs/UnpinTabs return false/fail safely
  EXPECT_FALSE(manager_.PinTabs({}, GlicPinTrigger::kUnknown));
  EXPECT_FALSE(manager_.UnpinTabs({}, GlicUnpinTrigger::kUnknown));

  // Verify UnpinAllTabs doesn't crash
  manager_.UnpinAllTabs(GlicUnpinTrigger::kUnknown);

  // Verify GetPinnedTabUsage returns nullopt
  EXPECT_EQ(manager_.GetPinnedTabUsage(tabs::TabHandle()), std::nullopt);

  // Verify getters return 0/false/empty
  EXPECT_EQ(manager_.GetMaxPinnedTabs(), 0);
  EXPECT_EQ(manager_.GetNumPinnedTabs(), 0);
  EXPECT_FALSE(manager_.IsTabPinned(tabs::TabHandle()));
  EXPECT_TRUE(manager_.GetPinnedTabs().empty());

  // Verify SetMaxPinnedTabs returns 0
  EXPECT_EQ(manager_.SetMaxPinnedTabs(5), 0);

  // Verify GetContextFromTab returns error
  base::test::TestFuture<GlicGetContextResult> get_context_future;
  manager_.GetContextFromTab(tabs::TabHandle(), {},
                             get_context_future.GetCallback());
  GlicGetContextResult context_result = get_context_future.Take();
  EXPECT_FALSE(context_result.has_value());
  EXPECT_EQ(context_result.error().error_code,
            GlicGetContextFromTabError::kPageContextNotEligible);

  // Verify GetFocusedBrowser returns nullptr
  EXPECT_EQ(manager_.GetFocusedBrowser(), nullptr);
}

IN_PROC_BROWSER_TEST_F(GlicDelegatingSharingManagerBrowserTest,
                       DelegatedBehavior) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);

  // Open a tab and verify we can pin it via delegation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  tabs::TabHandle handle = tab->GetHandle();

  service->ToggleUI(browser(), false,
                    mojom::InvocationSource::kTopChromeButton);

  auto* instance = service->GetInstanceForActiveTab(browser());
  ASSERT_TRUE(instance);

  // Use the real sharing manager as delegation target.
  auto& real_manager = instance->host().sharing_manager();

  // Ensure the tab in unpinned (may be pinned by default)
  real_manager.UnpinTabs({handle}, GlicUnpinTrigger::kUnknown);
  EXPECT_FALSE(real_manager.IsTabPinned(handle));

  // Set delegate.
  manager_.SetDelegate(&real_manager);

  // Pin via delegating manager.
  EXPECT_TRUE(manager_.PinTabs({handle}, GlicPinTrigger::kUnknown));

  // Verify state mirrored.
  EXPECT_TRUE(manager_.IsTabPinned(handle));
  EXPECT_TRUE(real_manager.IsTabPinned(handle));

  // Verify listeners work.
  base::test::TestFuture<tabs::TabInterface*, bool> pin_future;
  auto subscription = manager_.AddTabPinningStatusChangedCallback(
      pin_future.GetRepeatingCallback());

  // Unpin via real manager, should notify delegating manager's listeners.
  real_manager.UnpinTabs({handle}, GlicUnpinTrigger::kUnknown);

  auto [result_tab, result_pinned] = pin_future.Take();
  EXPECT_EQ(result_tab, tab);
  EXPECT_FALSE(result_pinned);

  // Unset delegate.
  manager_.SetDelegate(nullptr);

  // Verify manager_ is now empty/disconnected.
  EXPECT_FALSE(manager_.IsTabPinned(handle));

  // Real manager might still have state (if we hadn't unpinned it, or if we
  // repin it now). Let's repin on real manager to prove separation.
  real_manager.PinTabs({handle}, GlicPinTrigger::kUnknown);
  EXPECT_TRUE(real_manager.IsTabPinned(handle));
  EXPECT_FALSE(manager_.IsTabPinned(handle));
}

}  // namespace
}  // namespace glic
