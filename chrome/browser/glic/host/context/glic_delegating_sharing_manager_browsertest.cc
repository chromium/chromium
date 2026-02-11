// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_pinned_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class GlicDelegatingSharingManagerBrowserTest : public NonInteractiveGlicTest {
 public:
  GlicDelegatingSharingManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicMultiInstance,
         mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines},
        {});
  }
  ~GlicDelegatingSharingManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    NonInteractiveGlicTest::SetUpOnMainThread();
    // No delegate initially.
  }

  void TearDownOnMainThread() override {
    manager_.SetDelegate(nullptr);
    NonInteractiveGlicTest::TearDownOnMainThread();
  }

  // Setup tabs for test and return handles. Uses current tab, but if count > 1
  // then additional tabs will be created.
  std::vector<tabs::TabHandle> SetupTabs(int count) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    for (int i = 0; i < count - 1; ++i) {
      EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
          browser(), GURL("about:blank"),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    }
    TabStripModel* tab_strip = browser()->tab_strip_model();

    std::vector<tabs::TabHandle> handles;
    for (int i = 0; i < count; ++i) {
      tabs::TabInterface* tab =
          tabs::TabInterface::GetFromContents(tab_strip->GetWebContentsAt(i));
      EXPECT_TRUE(tab);
      if (tab) {
        handles.push_back(tab->GetHandle());
      }
    }
    return handles;
  }

 protected:
  GlicDelegatingSharingManager manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  std::vector<tabs::TabHandle> handles = SetupTabs(1);
  ASSERT_FALSE(handles.empty());
  tabs::TabHandle handle = handles[0];
  tabs::TabInterface* tab = handle.Get();
  ASSERT_TRUE(tab);

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

IN_PROC_BROWSER_TEST_F(GlicDelegatingSharingManagerBrowserTest,
                       PinnedTabSubscriptionForwarding) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);

  // Open a tab and verify we can pin it via delegation.
  std::vector<tabs::TabHandle> handles = SetupTabs(1);
  ASSERT_FALSE(handles.empty());
  tabs::TabHandle handle = handles[0];
  tabs::TabInterface* tab = handle.Get();
  ASSERT_TRUE(tab);

  service->ToggleUI(browser(), false,
                    mojom::InvocationSource::kTopChromeButton);

  auto* instance = service->GetInstanceForActiveTab(browser());
  ASSERT_TRUE(instance);

  // Use the real sharing manager as delegation target.
  auto& real_manager = instance->host().sharing_manager();

  // Ensure the tab is unpinned initially.
  real_manager.UnpinTabs({handle}, GlicUnpinTrigger::kUnknown);
  EXPECT_FALSE(real_manager.IsTabPinned(handle));

  // Set delegate.
  manager_.SetDelegate(&real_manager);

  // Subscribe to pinned tab events on delegating manager.
  base::test::TestFuture<tabs::TabInterface*, GlicPinningStatusEvent>
      pin_event_future;
  auto pin_event_sub = manager_.AddTabPinningStatusEventCallback(
      pin_event_future.GetRepeatingCallback());

  base::test::TestFuture<const std::vector<content::WebContents*>&>
      pinned_tabs_future;
  auto pinned_tabs_sub = manager_.AddPinnedTabsChangedCallback(
      pinned_tabs_future.GetRepeatingCallback());

  base::test::TestFuture<std::string, TabDataChangeCauseSet> tab_data_future;
  auto tab_data_sub = manager_.AddPinnedTabDataChangedCallback(
      base::BindLambdaForTesting([&](const TabDataChange& change) {
        tab_data_future.SetValue(change.tab_data->title.value_or(""),
                                 change.causes);
      }));

  // 1. PIN via manager_ (delegating)
  EXPECT_TRUE(manager_.PinTabs({handle}, GlicPinTrigger::kContextMenu));

  // Verify Pin Event
  auto [pin_event_tab, pin_event] = pin_event_future.Take();
  EXPECT_EQ(pin_event_tab, tab);
  ASSERT_TRUE(std::holds_alternative<GlicPinEvent>(pin_event));
  EXPECT_EQ(std::get<GlicPinEvent>(pin_event).trigger,
            GlicPinTrigger::kContextMenu);

  // Verify Pinned Tabs List Changed
  auto pinned_tabs = pinned_tabs_future.Take();
  EXPECT_EQ(pinned_tabs.size(), 1u);
  EXPECT_EQ(pinned_tabs[0], tab->GetContents());

  // 2. MODIFY Tab Title (triggers PinnedTabDataChanged)
  std::u16string new_title = u"New Title";
  content::TitleWatcher title_watcher(tab->GetContents(), new_title);
  ASSERT_TRUE(
      content::ExecJs(tab->GetContents(), "document.title = 'New Title';"));
  std::u16string actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(actual_title, new_title);

  // Verify PinnedTabDataChangedCallback fired.
  // Note: TabDataChange might fire multiple times or for other reasons, but we
  // expect at least one associated with title or generic update.
  auto [title, causes] = tab_data_future.Take();
  EXPECT_EQ(title, "New Title");
  EXPECT_TRUE(causes.Has(TabDataChangeCause::kTitle));

  // 3. UNPIN via delegating manager
  EXPECT_TRUE(manager_.UnpinTabs({handle}, GlicUnpinTrigger::kChip));

  // Verify Unpin Event
  auto [unpin_event_tab, unpin_event] = pin_event_future.Take();
  EXPECT_EQ(unpin_event_tab, tab);
  ASSERT_TRUE(std::holds_alternative<GlicUnpinEvent>(unpin_event));
  EXPECT_EQ(std::get<GlicUnpinEvent>(unpin_event).trigger,
            GlicUnpinTrigger::kChip);

  // Verify Pinned Tabs List Changed (empty)
  auto pinned_tabs_empty = pinned_tabs_future.Take();
  EXPECT_TRUE(pinned_tabs_empty.empty());
}

IN_PROC_BROWSER_TEST_F(GlicDelegatingSharingManagerBrowserTest,
                       FocusedTabSubscriptionForwarding) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);

  // Open a tab.
  std::vector<tabs::TabHandle> handles = SetupTabs(1);
  ASSERT_FALSE(handles.empty());
  tabs::TabHandle handle1 = handles[0];
  tabs::TabInterface* tab1 = handle1.Get();
  ASSERT_TRUE(tab1);

  service->ToggleUI(browser(), false,
                    mojom::InvocationSource::kTopChromeButton);

  GlicInstance* instance = service->GetInstanceForActiveTab(browser());
  ASSERT_TRUE(instance);
  auto& real_manager = instance->host().sharing_manager();

  // Ensure clean state.
  real_manager.UnpinAllTabs(GlicUnpinTrigger::kUnknown);
  manager_.SetDelegate(&real_manager);

  // Subscribe to focused selection changes.
  base::test::TestFuture<bool, tabs::TabInterface*> focus_future;
  auto focus_sub = manager_.AddFocusedTabChangedCallback(
      base::BindLambdaForTesting([&](const FocusedTabData& data) {
        if (!focus_future.IsReady()) {
          focus_future.SetValue(data.is_focus(), data.focus());
        }
      }));

  base::test::TestFuture<std::string> filtered_tab_data_future;
  auto tab_data_sub = manager_.AddFocusedTabDataChangedCallback(
      base::BindLambdaForTesting([&](const mojom::TabData* data) {
        if (data && data->title.value_or("") == "Focused Title" &&
            !filtered_tab_data_future.IsReady()) {
          filtered_tab_data_future.SetValue(data->title.value_or(""));
        }
      }));

  base::test::TestFuture<tabs::TabInterface*, GlicPinningStatusEvent>
      pin_event_future;
  auto pin_event_sub =
      manager_.AddTabPinningStatusEventCallback(base::BindLambdaForTesting(
          [&](tabs::TabInterface* tab, GlicPinningStatusEvent event) {
            if (!pin_event_future.IsReady()) {
              pin_event_future.SetValue(tab, std::move(event));
            }
          }));

  // 1. Initial State: Tab 1 active but NOT pinned. Focus should be empty/error.
  EXPECT_FALSE(manager_.GetFocusedTabData().is_focus());

  // 2. Pin Tab 1. Should become focused (Active + Pinned).
  manager_.PinTabs({handle1}, GlicPinTrigger::kUnknown);

  ASSERT_TRUE(pin_event_future.Wait());
  pin_event_future.Clear();

  // Check if focus updated successfully.
  auto [is_focus1, focus_tab1] = focus_future.Take();
  EXPECT_TRUE(is_focus1);
  EXPECT_EQ(focus_tab1, tab1);

  // 3. Unpin Tab 1. Should lose focus.
  manager_.UnpinTabs({handle1}, GlicUnpinTrigger::kUnknown);

  ASSERT_TRUE(pin_event_future.Wait());
  pin_event_future.Clear();

  auto [is_focus2, focus_tab2] = focus_future.Take();
  EXPECT_FALSE(is_focus2);

  // 4. Repin Tab 1. Regain focus.
  manager_.PinTabs({handle1}, GlicPinTrigger::kUnknown);
  auto [is_focus3, focus_tab3] = focus_future.Take();
  EXPECT_TRUE(is_focus3);
  EXPECT_EQ(focus_tab3, tab1);

  // 5. Open new Tab 2 (Active). Not pinned. Should lose focus.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* tab2 = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(tab1, tab2);
  tabs::TabHandle handle2 = tab2->GetHandle();

  auto [is_focus4, focus_tab4] = focus_future.Take();
  EXPECT_FALSE(is_focus4);

  // 6. Pin Tab 2. Should gain focus.
  manager_.PinTabs({handle2}, GlicPinTrigger::kUnknown);
  auto [is_focus5, focus_tab5] = focus_future.Take();
  EXPECT_TRUE(is_focus5);
  EXPECT_EQ(focus_tab5, tab2);

  // 7. Verify Data Change (Title) on focused tab and confirm the JS operation
  // worked.
  std::u16string new_title = u"Focused Title";
  content::TitleWatcher title_watcher(tab2->GetContents(), new_title);
  ASSERT_TRUE(content::ExecJs(tab2->GetContents(),
                              "document.title = 'Focused Title';"));
  std::u16string actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(actual_title, new_title);

  // Verify FocusedTabDataChanged callback.
  // Note: We might get multiple updates, we care that we eventually get the
  // title.
  ASSERT_TRUE(filtered_tab_data_future.Wait());
  auto title = filtered_tab_data_future.Take();
  EXPECT_EQ(title, "Focused Title");
}

IN_PROC_BROWSER_TEST_F(GlicDelegatingSharingManagerBrowserTest,
                       DelegateSwapTriggersPinNotifications) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);

  // Create 5 tabs.
  std::vector<tabs::TabHandle> handles = SetupTabs(5);
  ASSERT_EQ(handles.size(), 5u);

  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Setup manager 1.
  tab_strip->ActivateTabAt(0);
  service->ToggleUI(browser(), /*prevent_close=*/false,
                    mojom::InvocationSource::kTopChromeButton);
  auto* instance1 = service->GetInstanceForActiveTab(browser());
  ASSERT_TRUE(instance1);
  GlicSharingManager& manager1 = instance1->host().sharing_manager();

  // Pin tabs 0, 1, 2.
  manager1.UnpinAllTabs(GlicUnpinTrigger::kUnknown);
  manager1.PinTabs(
      std::vector<tabs::TabHandle>{handles[0], handles[1], handles[2]},
      GlicPinTrigger::kContextMenu);
  EXPECT_EQ(manager1.GetPinnedTabs().size(), 3u);

  // Setup manager 2.
  tab_strip->ActivateTabAt(3);
  service->ToggleUI(browser(), /*prevent_close=*/false,
                    mojom::InvocationSource::kTopChromeButton);
  auto* instance2 = service->GetInstanceForActiveTab(browser());
  ASSERT_TRUE(instance2);
  GlicSharingManager& manager2 = instance2->host().sharing_manager();

  // Ensure separate instances.
  ASSERT_NE(&manager1, &manager2);

  // Pin tabs 1, 3, 4.
  manager2.UnpinAllTabs(GlicUnpinTrigger::kUnknown);
  manager2.PinTabs(
      std::vector<tabs::TabHandle>{handles[1], handles[3], handles[4]},
      GlicPinTrigger::kContextMenu);
  EXPECT_EQ(manager2.GetPinnedTabs().size(), 3u);

  // Set delegate to manager 1.
  manager_.SetDelegate(&manager1);

  // Verify initial state.
  EXPECT_THAT(manager_.GetPinnedTabs(),
              testing::UnorderedElementsAre(handles[0].Get()->GetContents(),
                                            handles[1].Get()->GetContents(),
                                            handles[2].Get()->GetContents()));

  // Set up subscription for tab pinning status changes.
  base::test::TestFuture<tabs::TabInterface*, bool> pin_status_future(
      base::test::TestFutureMode::kQueue);
  auto pin_status_sub = manager_.AddTabPinningStatusChangedCallback(
      pin_status_future.GetRepeatingCallback());

  base::test::TestFuture<tabs::TabInterface*, GlicPinningStatusEvent>
      pin_event_future(base::test::TestFutureMode::kQueue);
  auto pin_event_sub = manager_.AddTabPinningStatusEventCallback(
      pin_event_future.GetRepeatingCallback());

  // Trigger delegate swap.
  manager_.SetDelegate(&manager2);

  // Verify triggered notifications in order.

  // 1. Verify Unpin notifications (tabs 0, 1, 2)
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(pin_status_future.Take(),
              std::make_tuple(handles[i].Get(), false));
    auto [tab, event] = pin_event_future.Take();
    EXPECT_EQ(tab, handles[i].Get());
    EXPECT_TRUE(std::holds_alternative<GlicUnpinEvent>(event));
  }

  // 2. Verify Pin notifications (tabs 1, 3, 4)
  const int pinned_indices[] = {1, 3, 4};
  for (int i : pinned_indices) {
    EXPECT_EQ(pin_status_future.Take(),
              std::make_tuple(handles[i].Get(), true));
    auto [tab, event] = pin_event_future.Take();
    EXPECT_EQ(tab, handles[i].Get());
    EXPECT_TRUE(std::holds_alternative<GlicPinEvent>(event));
  }

  // Ensure queues are empty.
  EXPECT_FALSE(pin_status_future.IsReady());
  EXPECT_FALSE(pin_event_future.IsReady());

  // Verify final state matches manager 2.
  EXPECT_FALSE(manager_.IsTabPinned(handles[0]));
  EXPECT_TRUE(manager_.IsTabPinned(handles[1]));
  EXPECT_FALSE(manager_.IsTabPinned(handles[2]));
  EXPECT_TRUE(manager_.IsTabPinned(handles[3]));
  EXPECT_TRUE(manager_.IsTabPinned(handles[4]));
}

IN_PROC_BROWSER_TEST_F(GlicDelegatingSharingManagerBrowserTest,
                       SetSameDelegateDoesNotTriggerPinNotifications) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  ASSERT_TRUE(service);

  // Create 3 tabs.
  std::vector<tabs::TabHandle> handles = SetupTabs(3);
  ASSERT_EQ(handles.size(), 3u);

  // Setup manager.
  service->ToggleUI(browser(), /*prevent_close=*/false,
                    mojom::InvocationSource::kTopChromeButton);
  auto* instance = service->GetInstanceForActiveTab(browser());
  ASSERT_TRUE(instance);
  GlicSharingManager& manager = instance->host().sharing_manager();

  // Pin tabs 0, 1.
  manager.UnpinAllTabs(GlicUnpinTrigger::kUnknown);
  manager.PinTabs(std::vector<tabs::TabHandle>{handles[0], handles[1]},
                  GlicPinTrigger::kContextMenu);
  EXPECT_EQ(manager.GetPinnedTabs().size(), 2u);

  manager_.SetDelegate(&manager);

  // Verify initial state.
  EXPECT_THAT(manager_.GetPinnedTabs(),
              testing::UnorderedElementsAre(handles[0].Get()->GetContents(),
                                            handles[1].Get()->GetContents()));

  // Setup subscription to consume status changes.
  bool callback_called = false;
  auto pin_status_sub = manager_.AddTabPinningStatusChangedCallback(
      base::BindLambdaForTesting([&](tabs::TabInterface* tab, bool pinned) {
        callback_called = true;
      }));

  auto pin_event_sub =
      manager_.AddTabPinningStatusEventCallback(base::BindLambdaForTesting(
          [&](tabs::TabInterface* tab, GlicPinningStatusEvent event) {
            callback_called = true;
          }));

  // Trigger SetDelegate with same manager.
  manager_.SetDelegate(&manager);

  // Verify no notifications were fired.
  EXPECT_FALSE(callback_called);

  // Verify state is still correct.
  EXPECT_THAT(manager_.GetPinnedTabs(),
              testing::UnorderedElementsAre(handles[0].Get()->GetContents(),
                                            handles[1].Get()->GetContents()));
}

class GlicStablePinningDelegatingSharingManagerBrowserTest
    : public NonInteractiveGlicTest {
 public:
  GlicStablePinningDelegatingSharingManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicMultiInstance,
         mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines},
        {});
  }

  std::unique_ptr<GlicSharingManagerImpl> CreateSharingManager(
      GlicPinnedTabManager* pinned_tab_manager) {
    return std::make_unique<GlicSharingManagerImpl>(
        std::make_unique<GlicEmptyFocusedTabManager>(),
        std::make_unique<GlicEmptyFocusedBrowserManager>(), pinned_tab_manager,
        browser()->profile(), /*metrics=*/nullptr);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b:479854184): make this work on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicStablePinningDelegatingSharingManagerBrowserTest,
                       StablePinningDelegateSwap) {
  // Subclass to access protected members for verification.
  class TestStableManager : public GlicStablePinningDelegatingSharingManager {
   public:
    using GlicDelegatingSharingManagerBase::GetDelegate;
    using GlicStablePinningDelegatingSharingManager::
        GlicStablePinningDelegatingSharingManager;
  };

  GlicEmptyPinnedTabManager pinned_mgr1;
  GlicEmptyPinnedTabManager pinned_mgr2;

  auto manager1 = CreateSharingManager(&pinned_mgr1);
  auto manager3 = CreateSharingManager(&pinned_mgr1);

  // Note: manager2 has a different pinned tab manager.
  auto manager2 = CreateSharingManager(&pinned_mgr2);

  TestStableManager stable_manager(manager1.get());

  // Swapping to manager3 should succeed (same pinned manager).
  stable_manager.SetDelegate(manager3.get());
  EXPECT_EQ(stable_manager.GetDelegate(), manager3.get());

  // Swapping to manager2 should crash (different pinned manager).
  EXPECT_CHECK_DEATH(stable_manager.SetDelegate(manager2.get()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Spy class to intercept window activation changes.
class SpyFocusedBrowserManager : public GlicEmptyFocusedBrowserManager {
 public:
  void OnGlicWindowActivationChanged(bool active) override {
    last_activation_change_ = active;
    GlicEmptyFocusedBrowserManager::OnGlicWindowActivationChanged(active);
  }

  std::optional<bool> last_activation_change() const {
    return last_activation_change_;
  }

 private:
  std::optional<bool> last_activation_change_;
};

IN_PROC_BROWSER_TEST_F(GlicStablePinningDelegatingSharingManagerBrowserTest,
                       SetDelegatePropagatesWindowActivation) {
  GlicEmptyPinnedTabManager pinned_mgr;

  // Setup first delegate with spy.
  auto focused_browser_manager1 = std::make_unique<SpyFocusedBrowserManager>();
  SpyFocusedBrowserManager* spy1 = focused_browser_manager1.get();
  auto manager1 = std::make_unique<GlicSharingManagerImpl>(
      std::make_unique<GlicEmptyFocusedTabManager>(),
      std::move(focused_browser_manager1), &pinned_mgr, browser()->profile(),
      /*metrics=*/nullptr);

  GlicStablePinningDelegatingSharingManager stable_manager(manager1.get());

  // Trigger activation change.
  stable_manager.OnGlicWindowActivationChanged(true);
  EXPECT_EQ(spy1->last_activation_change(), true);

  // Create second delegate with spy.
  auto focused_browser_manager2 = std::make_unique<SpyFocusedBrowserManager>();
  SpyFocusedBrowserManager* spy2 = focused_browser_manager2.get();
  auto manager2 = std::make_unique<GlicSharingManagerImpl>(
      std::make_unique<GlicEmptyFocusedTabManager>(),
      std::move(focused_browser_manager2), &pinned_mgr, browser()->profile(),
      /*metrics=*/nullptr);

  // Set delegate. Should propagate cached "true" state.
  stable_manager.SetDelegate(manager2.get());
  EXPECT_EQ(spy2->last_activation_change(), true);

  // Trigger deactivation.
  stable_manager.OnGlicWindowActivationChanged(false);
  EXPECT_EQ(spy2->last_activation_change(), false);

  // Switch back. Should propagate cached "false" state.
  stable_manager.SetDelegate(manager1.get());
  EXPECT_EQ(spy1->last_activation_change(), false);
}

}  // namespace
}  // namespace glic
