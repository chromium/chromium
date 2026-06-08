// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: Avoid using RunTestSequence unless absolutely necessary. Simple
// synchronous operations should be called directly to keep tests easy to read
// and debug. When waiting is required, `base::test::RunUntil` is usually
// sufficient and simpler than a full `RunTestSequence`.

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_tab_restore_data.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_invoke_handler.h"
#include "chrome/browser/glic/service/glic_invoke_task.h"
#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/base_window.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

#if BUILDFLAG(IS_ANDROID)

#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"
#else
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace glic {

class GlicInstanceCoordinatorBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicInstanceCoordinatorBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlicDaisyChainNewTabs, {}},
            {features::kGlicWebContentsWarming,
             {
                 // Speeds up tests to have quicker warming.
                 {features::kGlicWebContentsWarmingDelay.name, "2s"},
             }},
            {enterprise_reporting::kGeminiInChromeUsageReporting, {}},
        },
        /*disabled_features=*/{features::kGlicDefaultToLastActiveConversation});
  }
  ~GlicInstanceCoordinatorBrowserTest() override = default;

  void SetUpOnMainThread() override {
    GlicBrowserTestMixin::SetUpOnMainThread();
  }

  void RestoreMostRecentTab() {
#if BUILDFLAG(IS_ANDROID)
    TabModel* tab_model = static_cast<TabModel*>(GetTabListInterface());
    sessions::LiveTabContext* live_tab_context = tab_model->GetLiveTabContext();
    sessions::TabRestoreService* service =
        TabRestoreServiceFactory::GetForProfile(GetProfile());
    service->RestoreMostRecentEntry(live_tab_context);
#else
    chrome::RestoreTab(PlatformBrowserTest::browser());
#endif
  }

 protected:
  static InvokeWithAutoSubmitPasskey GetPassKey() {
    return InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, InitialState) {
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ToggleCreatesInstance) {
  ToggleGlicForActiveTab();
  auto result = WaitForGlicOpen();
  EXPECT_OK(result);
  auto* instance = result.value();
  EXPECT_OK(
      WaitForWebUiContentsVisibility(instance, content::Visibility::VISIBLE));
}

// ClearPrimaryAccount is not supported on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SignOutClosesAllInstances DISABLED_SignOutClosesAllInstances
#else
#define MAYBE_SignOutClosesAllInstances SignOutClosesAllInstances
#endif
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       MAYBE_SignOutClosesAllInstances) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 1u);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(identity_manager);

  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    signin::MakePrimaryAccountAvailable(identity_manager, "test@gmail.com",
                                        signin::ConsentLevel::kSignin);
  }

  signin::ClearPrimaryAccount(identity_manager);

  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, CloseHidesInstance) {
  ToggleGlicForActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, WaitForGlicOpen());

  PreventDeletionOnClose(instance, "test_conversation");
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose());
  EXPECT_FALSE(instance->IsShowing());
  EXPECT_OK(
      WaitForWebUiContentsVisibility(instance, content::Visibility::HIDDEN));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       KeptBoundWhenFlagDisabled) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK(OpenGlicForActiveTab());

  // Do not submit any input.

  // Close Glic for the tab.
  ASSERT_OK(CloseGlicForTabAndWait(tab));

  // Because features::kGlicDefaultToLastActiveConversation is disabled in this
  // suite, it should NOT unbind from the tab.
  auto* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);
  EXPECT_EQ(GetContentsVisibility(instance), content::Visibility::HIDDEN);
}

class GlicInstanceCoordinatorUnbindOnCloseTest
    : public GlicInstanceCoordinatorBrowserTest {
 public:
  GlicInstanceCoordinatorUnbindOnCloseTest() {
    feature_list_.InitWithFeatures(
        {kGlicUnbindOnClose, features::kGlicDefaultToLastActiveConversation},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/514777907): Re-enable this test.
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorUnbindOnCloseTest,
                       DISABLED_UnboundWhenNoInputSubmitted) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());

  // Submit input on tab1 to keep it bound when closed.
  PreventDeletionOnClose(instance1, "test_conversation_1");

  // Close Glic for tab1. It stays bound because of input.
  ASSERT_OK(CloseGlicForTabAndWait(tab1));
  ASSERT_EQ(GetInstanceForTab(tab1), instance1);

  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  // Open Glic for tab2. It should reuse instance1.
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());
  EXPECT_EQ(instance1, instance2);

  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 1u);
  EXPECT_TRUE(instance2->IsShowing());

  // Do not submit any input on tab2, close the side panel for the active tab
  // (tab2).
  ASSERT_OK(CloseGlicForTabAndWait(tab2));

  // Because no input was submitted on tab2 and the flags are on, it should
  // unbind from tab2.
  EXPECT_FALSE(GetInstanceForTab(tab2));

  // But because it was bound to tab1 (and kept bound), the instance itself
  // should still exist.
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 1u);
  EXPECT_EQ(GetInstanceForTab(tab1), instance1);
  EXPECT_EQ(GetContentsVisibility(instance1), content::Visibility::HIDDEN);
}

// TODO(crbug.com/514816170): Re-enable when no longer flaky on Android and
// Windows.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_UnboundWhenClosedBySidePanelCoordinator \
  DISABLED_UnboundWhenClosedBySidePanelCoordinator
#else
#define MAYBE_UnboundWhenClosedBySidePanelCoordinator \
  UnboundWhenClosedBySidePanelCoordinator
#endif
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorUnbindOnCloseTest,
                       MAYBE_UnboundWhenClosedBySidePanelCoordinator) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());

  // Submit input on tab1 to keep it bound when closed.
  PreventDeletionOnClose(instance1, "test_conversation_1");

  // Close Glic for tab1. It stays bound because of input.
  ASSERT_OK(CloseGlicForTabAndWait(tab1));
  ASSERT_EQ(GetInstanceForTab(tab1), instance1);

  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  // Open Glic for tab2. It should reuse instance1.
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());
  EXPECT_EQ(instance1, instance2);

  // Do not submit any input on tab2.

  GlicSidePanelCoordinator::GetForTab(tab2)->Close();

  // Wait for the side panel to be closed.
  ASSERT_OK(
      WaitForSidePanelState(tab2, GlicSidePanelCoordinator::State::kClosed));

  // Because no input was submitted on tab2 and the flags are on, it should
  // unbind from tab2.
  EXPECT_FALSE(GetInstanceForTab(tab2));

  // But because it was bound to tab1 (and kept bound), the instance itself
  // should still exist.
  EXPECT_EQ(GetInstanceForTab(tab1), instance1);
  EXPECT_EQ(GetContentsVisibility(instance1), content::Visibility::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorUnbindOnCloseTest,
                       KeptBoundWhenInputSubmitted) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());

  PreventDeletionOnClose(instance1, "test_conversation_1");
  ASSERT_OK(CloseGlicForTabAndWait(tab1));

  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());
  EXPECT_EQ(instance1, instance2);

  // Simulate user input on tab2.
  instance2->OnUserInputSubmitted(mojom::WebClientMode::kText);

  // Close the side panel for the active tab (tab2).
  ASSERT_OK(CloseGlicForTabAndWait(tab2));

  // Because input was submitted, it should NOT unbind from tab2.
  EXPECT_TRUE(GetInstanceForTab(tab2));
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 1u);
  EXPECT_FALSE(instance2->IsShowing());
  EXPECT_EQ(GetContentsVisibility(instance2), content::Visibility::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorUnbindOnCloseTest,
                       KeptBoundWhenPinnedViaConversationChange) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  // Manually pin tab2 with kConversationChange.
  instance->sharing_manager().PinTabs({tab2->GetHandle()},
                                      GlicPinTrigger::kConversationChange);

  // Bind tab2 to the instance by showing it.
  coordinator().ShowInstanceForTabs({tab2}, instance->id());

  // Verify it is bound.
  EXPECT_EQ(GetInstanceForTab(tab2), instance);

  // Do not submit any input on tab2.

  // Close Glic for tab2.
  ASSERT_OK(CloseGlicForTabAndWait(tab2));

  // Because it was pinned with kConversationChange (not kInstanceCreation),
  // it should NOT unbind from tab2, even though no input was submitted.
  auto* instance2 = GetInstanceForTab(tab2);
  ASSERT_TRUE(instance2);
  EXPECT_EQ(GetContentsVisibility(instance2), content::Visibility::HIDDEN);
}

// TODO(b/521445431): Re-enable this test after fixing the flakes.
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorUnbindOnCloseTest,
                       DISABLED_KeptBoundWhenInPlaceConversationSwitched) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  // Explicitly pin tab2 to the instance's sharing manager with
  // kInstanceCreation.
  instance->sharing_manager().PinTabs({tab2->GetHandle()},
                                      GlicPinTrigger::kInstanceCreation);

  // Verify both are pinned with kInstanceCreation.
  auto usage_tab1_before =
      instance->sharing_manager().GetPinnedTabUsage(tab1->GetHandle());
  ASSERT_TRUE(usage_tab1_before);
  EXPECT_EQ(usage_tab1_before->pin_event.trigger,
            GlicPinTrigger::kInstanceCreation);

  auto usage_tab2_before =
      instance->sharing_manager().GetPinnedTabUsage(tab2->GetHandle());
  ASSERT_TRUE(usage_tab2_before);
  EXPECT_EQ(usage_tab2_before->pin_event.trigger,
            GlicPinTrigger::kInstanceCreation);

  // Perform an in-place conversation switch on tab2.
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = "switched_conversation_id";
  info->conversation_title = "Switched Conversation";

  base::test::TestFuture<std::optional<mojom::SwitchConversationErrorReason>>
      switch_future;
  coordinator().SwitchConversation(*instance, ShowOptions::ForSidePanel(*tab2),
                                   std::move(info),
                                   switch_future.GetCallback());
  EXPECT_EQ(switch_future.Get(), std::nullopt);

  // Verify BOTH tab1 and tab2 triggers are updated to kConversationChange.
  auto usage_tab1_after =
      instance->sharing_manager().GetPinnedTabUsage(tab1->GetHandle());
  ASSERT_TRUE(usage_tab1_after);
  EXPECT_EQ(usage_tab1_after->pin_event.trigger,
            GlicPinTrigger::kConversationChange);

  auto usage_tab2_after =
      instance->sharing_manager().GetPinnedTabUsage(tab2->GetHandle());
  ASSERT_TRUE(usage_tab2_after);
  EXPECT_EQ(usage_tab2_after->pin_event.trigger,
            GlicPinTrigger::kConversationChange);

  // Do not submit any input. Close Glic for tab2.
  ASSERT_OK(CloseGlicForTabAndWait(tab2));

  // Verify both stay bound!
  EXPECT_TRUE(GetInstanceForTab(tab1));
  EXPECT_TRUE(GetInstanceForTab(tab2));
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorUnbindOnCloseTest,
                       KeptBoundWhenPinnedViaContextMenu) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  // This pins with kContextMenu.
  coordinator().CreateNewConversationForTabs({tab1, tab2});
  auto* instance = coordinator().GetInstanceImplForTab(tab2);
  ASSERT_TRUE(instance);
  EXPECT_EQ(GetInstanceForTab(tab1), instance);
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 1u);
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_EQ(GetContentsVisibility(instance), content::Visibility::VISIBLE);

  // Do not submit any input, close the side panel for the active tab (tab2).
  ASSERT_OK(CloseGlicForTabAndWait(tab2));

  // Because it was pinned with kContextMenu (not kInstanceCreation),
  // it should NOT unbind from tab2, even though no input was submitted.
  EXPECT_TRUE(GetInstanceForTab(tab2));
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 1u);
  EXPECT_FALSE(instance->IsShowing());
  EXPECT_EQ(GetContentsVisibility(instance), content::Visibility::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorUnbindOnCloseTest,
                       PreventUafOnReentrantUnbind) {
  // Disable the keep side panel open on new tabs setting to prevent a new
  // GlicInstance from being created automatically when we create tab2 below.
  GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // Create a new tab to push tab1 into the background, transitioning its
  // side panel coordinator state to kBackgrounded.
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab2);

  // Wait until T1 transitions to kBackgrounded.
  ASSERT_OK(WaitForSidePanelState(
      tab1, GlicSidePanelCoordinator::State::kBackgrounded));

  // Verify the active instance is still the one bound to tab1.
  EXPECT_EQ(GetInstanceForTab(tab1), instance);

  base::WeakPtr<GlicInstanceImpl> weak_instance = instance->GetWeakPtr();

  // Manually trigger UnbindEmbedder on tab1. Since it is backgrounded,
  // CloseInternal() will hide it synchronously and recursively trigger
  // DidCloseFor() -> UnbindEmbedder() in a re-entrant frame, deleting
  // the instance. The outer frame will safely return early via the
  // base::WeakPtr guard.
  instance->UnbindEmbedder(EmbedderKey(tab1));

  ASSERT_OK(RunUntilEqual<GlicInstanceImpl*>(
      [&]() { return weak_instance.get(); }, nullptr));

  // Verify that the instance was successfully deleted and no UAF occurred.
  EXPECT_FALSE(weak_instance);
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       CreateConversationForTabs) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  coordinator().CreateNewConversationForTabs({tab1, tab2});

  EXPECT_TRUE(GetInstanceForTab(tab1));
  EXPECT_EQ(GetInstanceForTab(tab1), GetInstanceForTab(tab2));
  EXPECT_TRUE(GetInstanceForTab(tab1)->IsShowing());
  EXPECT_FALSE(coordinator().GetInstancesForTesting().empty());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       NewTabDaisyChaining) {
  GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, true);

  ASSERT_OK(OpenGlicForActiveTab());

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateUserInitiatedTab(GURL("about:blank"));

  EXPECT_TRUE(GetInstanceForTab(tab1));
  auto* tab2_instance = coordinator().GetInstanceImplForTab(tab2);
  EXPECT_TRUE(tab2_instance);
  EXPECT_NE(GetInstanceForTab(tab1), tab2_instance);
  EXPECT_OK(WaitForEmbedderActivationOrPeek(tab2_instance, tab2));

  GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);
  tabs::TabInterface* tab3 = CreateUserInitiatedTab(GURL("about:blank"));
  EXPECT_FALSE(GetInstanceForTab(tab3));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ShowInstanceForTabs) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  EXPECT_NE(instance1, instance2);

  // Assign a conversation ID to instance2 so it can be targeted.
  // In production, this comes from the web client.
  PreventDeletionOnClose(instance2, "conv_2");

  // Move tab1 to instance2's conversation.
  coordinator().ShowInstanceForTabs({tab1}, instance2->id());

  EXPECT_EQ(GetInstanceForTab(tab1), instance2);
  EXPECT_EQ(GetInstanceForTab(tab2), instance2);
  EXPECT_EQ(GetContentsVisibility(instance2), content::Visibility::VISIBLE);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabContentsDaisyChaining) {
  // TODO(crbug.com/498990943): Failing on builder "android-11-x86-rel"
  SKIP_TEST_FOR_NON_DESKTOP_ANDROID();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // Case 1: Ctrl+Click (New Tab)
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/false);
    tabs::TabInterface* tab2 = waiter.Wait();

    EXPECT_EQ(instance, coordinator().GetInstanceImplForTab(tab2));
    EXPECT_TRUE(tab1->IsActivated());

    // Verify side panel state for the background tab.
    EXPECT_OK(WaitForSidePanelState(
        tab2, GlicSidePanelCoordinator::State::kBackgrounded));

    // Activate the background tab and verify it is shown.
    tab2->GetContents()->GetDelegate()->ActivateContents(tab2->GetContents());
    ASSERT_OK(WaitForEmbedderActivationOrPeek(instance, tab2));
    // Verify focus stays on the page contents, not the side panel.
    EXPECT_FALSE(instance->HasFocus());
  }

  // Case 2: Shift+Click (New Window)
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    GetTabListInterface()->ActivateTab(tab1->GetHandle());
    SimulateLinkClick(tab1, /*ctrl_key=*/false, /*shift_key=*/true);

    tabs::TabInterface* tab3 = waiter.Wait();
    auto* new_window = tab3->GetBrowserWindowInterface();
    new_window->GetWindow()->Activate();

    EXPECT_EQ(instance, GetInstanceForTab(tab3));
    EXPECT_EQ(TabListInterface::From(new_window)->GetActiveTab(), tab3);
    ASSERT_OK(WaitForEmbedderActivationOrPeek(instance, tab3));
    // Focus should be on the new window's page contents.
    EXPECT_FALSE(instance->HasFocus());
  }

  // Case 3: Ctrl+Shift+Click (Foreground Tab)
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    tab1->GetBrowserWindowInterface()->GetWindow()->Activate();
    GetTabListInterface()->ActivateTab(tab1->GetHandle());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/true);
    tabs::TabInterface* tab4 = waiter.Wait();

    EXPECT_EQ(instance, GetInstanceForTab(tab4));
    EXPECT_EQ(GetTabListInterface()->GetActiveTab(), tab4);
    ASSERT_OK(WaitForEmbedderActivationOrPeek(instance, tab4));
    // Focus should be on the new foreground tab's page contents.
    EXPECT_FALSE(instance->HasFocus());
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       BookmarkDaisyChaining) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  // Simulate opening a bookmark in a new foreground tab.
  GlicTestTabAddedWaiter waiter(GetProfile());

#if BUILDFLAG(IS_ANDROID)
  TabListInterface* tab_list = GetTabListInterface();
  TabModel* tab_model = static_cast<TabModel*>(tab_list);
  Profile* profile = GetProfile();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  web_contents->GetController().LoadURL(
      GURL("about:blank"), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, std::string());

  tabs::TabInterface* tab2 = tab_model->CreateTab(
      nullptr, std::move(web_contents), -1,
      TabModel::TabLaunchType::FROM_BOOKMARK_BAR_BACKGROUND, false);
  tab_model->ActivateTab(tab2->GetHandle());
#else
  NavigateParams params(PlatformBrowserTest::browser(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
#endif

  tabs::TabInterface* tab2_result = waiter.Wait();

  EXPECT_EQ(instance, coordinator().GetInstanceImplForTab(tab2_result));
  EXPECT_OK(WaitForEmbedderActivationOrPeek(instance, tab2_result));
}

// Glic floaty and live modes are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       SidePanelActivationStopsFloatyListening) {
  // Open floaty and start listening
  coordinator().Toggle(/*browser=*/nullptr, /*prevent_close=*/true,
                       mojom::InvocationSource::kTopChromeButton);
  GlicInstanceImpl* floaty_instance =
      static_cast<GlicInstanceImpl*>(coordinator().GetActiveInstance());
  ASSERT_TRUE(floaty_instance);
  ASSERT_TRUE(floaty_instance->IsDetached());

  floaty_instance->host().OnMicrophoneStatusChanged(
      mojom::MicrophoneStatus::kListening);
  EXPECT_EQ(floaty_instance->host().microphone_status(),
            mojom::MicrophoneStatus::kListening);

  // Now open a new side panel
  tabs::TabInterface* tab2 =
      GetTabListInterface()->OpenTab(GURL("about:blank"), -1);
  GetTabListInterface()->ActivateTab(tab2->GetHandle());

  coordinator().Toggle(tab2->GetBrowserWindowInterface(),
                       /*prevent_close=*/true,
                       mojom::InvocationSource::kTopChromeButton);
  ASSERT_OK_AND_ASSIGN(auto side_panel_instance, WaitForGlicOpen(tab2));
  ASSERT_EQ(side_panel_instance->GetPanelState().kind,
            mojom::PanelStateKind::kAttached);

  // Manually activate the side panel
  side_panel_instance->OnEmbedderWindowActivationChanged(true);
  EXPECT_EQ(coordinator().GetActiveInstance(), side_panel_instance);

  // Verify that Floaty has stopped listening
  floaty_instance->host().OnMicrophoneStatusChanged(
      mojom::MicrophoneStatus::kNotListening);
  EXPECT_EQ(floaty_instance->host().microphone_status(),
            mojom::MicrophoneStatus::kNotListening);
}
#endif

// Flaky test. crbug.com/498990943
IN_PROC_BROWSER_TEST_F(
    GlicInstanceCoordinatorBrowserTest,
    DISABLED_TabContentsDaisyChainingSuppressedWhenUnifiedFreInProgress) {
  SetFRECompletion(GetProfile(), prefs::FreStatus::kIncomplete);
  ASSERT_OK(OpenGlicForActiveTab());
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // Try to daisy chain via Page Contents
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/false);
    tabs::TabInterface* tab2 = waiter.Wait();

    GlicInstance* tab2_instance = GetInstanceForTab(tab2);

    // Verify daisy chaining did not occur
    EXPECT_EQ(nullptr, tab2_instance);
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       WebClientLinkClickDaisyChaining) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // Case 1: Create Foreground Tab
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    instance->CreateTab(GetSimpleTestUrl(),
                        /*open_in_background=*/false,
                        /*window_id=*/std::nullopt, base::DoNothing());
    tabs::TabInterface* tab2 = waiter.Wait();

    EXPECT_EQ(instance, GetInstanceForTab(tab2));
    EXPECT_OK(WaitForEmbedderActivationOrPeek(instance, tab2));
    EXPECT_EQ(GetTabListInterface()->GetActiveTab(), tab2);
    // The glic embedder should not have focus when daisy chaining
    EXPECT_FALSE(instance->HasFocus());
  }

  // Case 2: Create Background Tab
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    instance->CreateTab(GetSimpleTestUrl(),
                        /*open_in_background=*/true,
                        /*window_id=*/std::nullopt, base::DoNothing());
    tabs::TabInterface* tab3 = waiter.Wait();

    EXPECT_EQ(instance, GetInstanceForTab(tab3));
    // Active tab should still be previously active tab (tab2)
    EXPECT_NE(GetTabListInterface()->GetActiveTab(), tab3);

    EXPECT_OK(WaitForSidePanelState(
        tab3, GlicSidePanelCoordinator::State::kBackgrounded));

    ActivateTab(tab3);
    EXPECT_OK(WaitForEmbedderActivationOrPeek(instance, tab3));
  }
}

// Glic floaty is not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       WebClientLinkClickDaisyChainingFromFloaty) {
  // Open floaty
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsDetached());

  // In order to really test this, the active tab needs to be one that's not
  // bound to the floaty instance. We can just create a new tab.
  tabs::TabInterface* unbound_tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_FALSE(GetInstanceForTab(unbound_tab));

  // Case 1: Create Foreground Tab
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    instance->CreateTab(GetSimpleTestUrl(),
                        /*open_in_background=*/false,
                        /*window_id=*/std::nullopt, base::DoNothing());
    tabs::TabInterface* tab2 = waiter.Wait();

    // The newly created tab should be bound to the floaty instance.
    EXPECT_EQ(instance, GetInstanceForTab(tab2));
    EXPECT_TRUE(instance->IsDetached());
    EXPECT_EQ(GetTabListInterface()->GetActiveTab(), tab2);
  }

  // Case 2: Create Background Tab
  {
    GetTabListInterface()->ActivateTab(unbound_tab->GetHandle());
    GlicTestTabAddedWaiter waiter(GetProfile());
    instance->CreateTab(GetSimpleTestUrl(),
                        /*open_in_background=*/true,
                        /*window_id=*/std::nullopt, base::DoNothing());
    tabs::TabInterface* tab3 = waiter.Wait();

    // It should be bound to the new tab, but the side panel shouldn't open.
    // Instead the floating UI remains detached.
    EXPECT_EQ(instance, GetInstanceForTab(tab3));
    EXPECT_TRUE(instance->IsDetached());
    // Active tab should still be previously active tab
    EXPECT_NE(GetTabListInterface()->GetActiveTab(), tab3);
  }
}
#endif

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ActiveEmbedderFollowsActiveTab) {
  // TODO: Tweak this to support peek. The assertions will need to be different,
  // and it's not worth trying to fix the flakes before then.
  SKIP_TEST_FOR_NON_DESKTOP_ANDROID();
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  coordinator().CreateNewConversationForTabs({tab1, tab2});
  auto* instance = coordinator().GetInstanceImplForTab(tab1);

  EXPECT_OK(WaitForActiveEmbedderToMatchTab(instance, tab2));

  // Switch back to tab 1.
  ActivateTab(tab1);
  EXPECT_OK(WaitForEmbedderActivationOrPeek(instance, tab1));

  // Switch to tab 2.
  ActivateTab(tab2);
  EXPECT_OK(WaitForEmbedderActivationOrPeek(instance, tab2));

  // Tab 1 shows peek or becomes active if tab2 is closed
  tab2->Close();
  EXPECT_OK(WaitForEmbedderActivationOrPeek(instance, tab1));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       DeactivationWhenSwitchingToUnboundTab) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab3 = CreateAndActivateTab(GURL("about:blank"));

  // Bind tab1 and tab3 to the same instance.a
  coordinator().CreateNewConversationForTabs({tab1, tab3});
  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_TRUE(instance);
  EXPECT_EQ(instance, coordinator().GetInstanceImplForTab(tab3));
  EXPECT_FALSE(coordinator().GetInstanceImplForTab(tab2));

  EXPECT_OK(
      WaitForSidePanelState(tab3, GlicSidePanelCoordinator::State::kShown));

  base::test::TestFuture<GlicInstance*> future;
  auto subscription =
      coordinator().AddActiveInstanceChangedCallbackAndNotifyImmediately(
          future.GetRepeatingCallback());

  // Verify initial state notification.
  ASSERT_EQ(future.Take(), instance);

  // Close Tab 3. Chrome should switch to Tab 2 (MRU).
  tab3->Close();

  // Verify Tab 2 is active.
  ASSERT_TRUE(tab2->IsActivated());

  // Verify deactivation event was received.
  EXPECT_EQ(future.Take(), nullptr);
  EXPECT_EQ(coordinator().GetActiveInstance(), nullptr);
  EXPECT_FALSE(instance->GetActiveEmbedderTabForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ExplicitPinningUsingShowInstanceForTabs) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  // Unpin the tab.
  instance->sharing_manager().UnpinTabs({tab->GetHandle()});
  EXPECT_FALSE(instance->sharing_manager().IsTabPinned(tab->GetHandle()));

  // Verify kContextMenu trigger explicitly pins the tab.
  coordinator().ShowInstanceForTabs({tab}, instance->id());
  EXPECT_TRUE(instance->sharing_manager().IsTabPinned(tab->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, TabRestoration) {
  // Add a new tab so we don't close the browser when we close the tab.
  auto* tab = CreateAndActivateTab(GURL("about:blank"));
  // Wait for contents to load to ensure that the tab will be eligible for
  // restoration.
  EXPECT_TRUE(content::WaitForLoadStop(tab->GetContents()));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto instance_id = instance->id();
  base::WeakPtr<GlicInstanceImpl> weak_instance = instance->GetWeakPtr();

  GetTabListInterface()->CloseTab(tab->GetHandle());
  // Wait for the asynchronous deletion to complete so that tab restoration
  // tests the actual restoration path (instead of reusing a still-living
  // instance).
  ASSERT_OK(WaitForInstanceDeletion(weak_instance));

  // Restore the tab.
  GlicTestTabAddedWaiter waiter(GetProfile());
  RestoreMostRecentTab();
  // Verify the restored tab is bound to the instance and the side panel is
  // open.
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  GlicInstance* restored_instance = nullptr;
  EXPECT_TRUE(base::test::RunUntil([&]() {
    restored_instance = GetInstanceForTab(restored_tab);
    return restored_instance != nullptr;
  }));

  EXPECT_EQ(restored_instance->id(), instance_id);
  EXPECT_TRUE(restored_instance->IsShowing());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabRestoration_PinnedButNotBound) {
  // Tab 1: Keep Instance 1 alive.
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance1, OpenGlicForActiveTab());
  auto instance1_id = instance1->id();

  // Tab 2: The test tab.
  auto* tab2 = CreateAndActivateTab(GURL("about:blank"));
  // Wait for contents to load to ensure that the tab will be eligible for
  // restoration.
  EXPECT_TRUE(content::WaitForLoadStop(tab2->GetContents()));

  // Bind Tab 2 to Instance 1.
  coordinator().ShowInstanceForTabs({tab2}, instance1_id);

  // Create Instance 2 and bind Tab 2 to it.
  // This should unbind Tab 2 from Instance 1, but keep it pinned to Instance 1.
  ASSERT_TRUE(instance1->sharing_manager().IsTabPinned(tab2->GetHandle()));
  coordinator().CreateNewConversationForTabs({tab2});
  GlicInstanceImpl* instance2 = GetInstanceForTab(tab2);
  ASSERT_TRUE(instance2);
  ASSERT_NE(instance2, instance1);
  // Set a conversation ID so we can verify restoration of conversation id.
  const std::string kConvId = "test_conversation_id";
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = kConvId;
  instance2->RegisterConversation(std::move(info), base::DoNothing());
  auto instance2_id = instance2->id();

  // Verify Tab 2 state before close.
  // Bound to Instance 2.
  ASSERT_EQ(GetInstanceForTab(tab2), instance2);
  // Pinned to Instance 2 (auto-pinned).
  ASSERT_TRUE(instance2->sharing_manager().IsTabPinned(tab2->GetHandle()));
  // Pinned to Instance 1.
  ASSERT_TRUE(instance1->sharing_manager().IsTabPinned(tab2->GetHandle()));
  GetTabListInterface()->ActivateTab(tab2->GetHandle());

  base::WeakPtr<GlicInstanceImpl> weak_instance2 = instance2->GetWeakPtr();

  // Close Tab 2.
  GetTabListInterface()->CloseTab(tab2->GetHandle());
  // Wait for the asynchronous deletion to complete so that tab restoration
  // tests the actual restoration path (instead of reusing a still-living
  // instance).
  ASSERT_OK(WaitForInstanceDeletion(weak_instance2));

  // Restore Tab 2.
  GlicTestTabAddedWaiter waiter(GetProfile());
  RestoreMostRecentTab();
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  ASSERT_OK_AND_ASSIGN(auto* restored_instance2,
                       WaitForGlicInstanceBoundToTab(restored_tab));
  // The newly created instance should have the same instance id and
  // conversation id as the original instance.
  ASSERT_EQ(instance2_id, restored_instance2->id());
  ASSERT_EQ(restored_instance2->conversation_id(), kConvId);
  // Should be pinned to Instance 2.
  ASSERT_TRUE(restored_instance2->sharing_manager().IsTabPinned(
      restored_tab->GetHandle()));
  // Should be pinned to Instance 1.
  ASSERT_TRUE(
      instance1->sharing_manager().IsTabPinned(restored_tab->GetHandle()));
  EXPECT_OK(WaitForEmbedderActivationOrPeek(restored_instance2, restored_tab));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabRestoration_RestoreToExistingInstance) {
  // Tab 1: Keep the instance alive.
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto instance_id = instance->id();

  // Tab 2: The one we will close and restore.
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  // Wait for contents to load to ensure that the tab will be eligible for
  // restoration.
  EXPECT_TRUE(content::WaitForLoadStop(tab2->GetContents()));
  coordinator().ShowInstanceForTabs({tab2}, instance_id);
  ASSERT_EQ(GetInstanceForTab(tab2), instance);

  // Close Tab 2.
  GetTabListInterface()->CloseTab(tab2->GetHandle());

  // The instance should still exist because Tab 1 keeps it alive.
  ASSERT_EQ(coordinator().GetInstanceImplFor(instance_id), instance);

  // Restore Tab 2.
  GlicTestTabAddedWaiter waiter(GetProfile());
  RestoreMostRecentTab();
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  ASSERT_OK_AND_ASSIGN(auto* bound_instance,
                       WaitForGlicInstanceBoundToTab(restored_tab));

  // Should have reused the existing instance.
  EXPECT_EQ(bound_instance, instance);
  EXPECT_OK(WaitForEmbedderActivationOrPeek(instance, restored_tab));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabRestoration_ConversationIdMismatchReturnsNull) {
  // Tab 1: Keep the instance alive.
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto instance_id = instance->id();

  // Set a conversation ID on the instance.
  const std::string kConvId = "test_conversation_id";
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = kConvId;
  instance->RegisterConversation(std::move(info), base::DoNothing());

  // Create a fake restore state for a new tab.
  // It will have the SAME instance ID but a DIFFERENT conversation ID.
  GlicRestoredState state;
  state.bound_instance.instance_id = instance_id.value();
  state.bound_instance.conversation_id = "different_conversation_id";
  state.side_panel_open = true;  // Try to open side panel, should be skipped.

  // Create a WebContents manually.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));

  // Attach the restore data.
  GlicTabRestoreData::CreateForWebContents(web_contents.get(),
                                           std::move(state));

  // Now add it to the tab strip.
  auto* tab_list = GetTabListInterface();
  tabs::TabInterface* restored_tab = nullptr;
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    tab_list->InsertWebContentsAt(-1, std::move(web_contents),
                                  /*should_pin=*/false, std::nullopt);
    restored_tab = waiter.Wait();
  }
  ASSERT_TRUE(restored_tab);

  // Verify that the restored tab is NOT bound to the instance.
  EXPECT_EQ(GetInstanceForTab(restored_tab), nullptr);

  // Verify that the side panel is NOT open for the new tab.
  EXPECT_OK(WaitForSidePanelState(restored_tab,
                                  GlicSidePanelCoordinator::State::kClosed));

  // Clean up the tab we created and wait for it to be destroyed to avoid
  // race conditions during test teardown.
  content::WebContentsDestroyedWatcher destroyer(restored_tab->GetContents());
  tab_list->CloseTab(restored_tab->GetHandle());
  destroyer.Wait();
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabRestoration_SidePanelClosed) {
  // Add a new tab so we don't close the browser when we close the tab.
  auto* tab = CreateAndActivateTab(GURL("about:blank"));
  // Wait for contents to load to ensure that the tab will be eligible for
  // restoration.
  EXPECT_TRUE(content::WaitForLoadStop(tab->GetContents()));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  PreventDeletionOnClose(instance);

  // Close the side panel for this tab.
  auto original_instance_id = instance->id();
  instance->Close(EmbedderKey(tab), CloseOptions());
  ASSERT_OK(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kClosed));

  base::WeakPtr<GlicInstanceImpl> weak_instance = instance->GetWeakPtr();

  // Close the tab.
  GetTabListInterface()->CloseTab(tab->GetHandle());
  // Wait for the asynchronous deletion to complete so that tab restoration
  // tests the actual restoration path (instead of reusing a still-living
  // instance).
  ASSERT_OK(WaitForInstanceDeletion(weak_instance));

  // Restore the tab.
  GlicTestTabAddedWaiter waiter(GetProfile());
  RestoreMostRecentTab();
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  ASSERT_OK_AND_ASSIGN(auto* bound_instance,
                       WaitForGlicInstanceBoundToTab(restored_tab));

  // Since it was closed (unbound) when we closed the tab, it should be restored
  // as unbound.
  EXPECT_EQ(original_instance_id, bound_instance->id());
  EXPECT_OK(WaitForSidePanelState(restored_tab,
                                  GlicSidePanelCoordinator::State::kClosed));
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       PrintAfterReloadDoesNotCrash) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // Trigger a reload.
  auto* glic_service = GlicKeyedService::Get(GetProfile());
  glic_service->Reload(
      instance->host().webui_contents()->GetPrimaryMainFrame());

  // Wait for the reload to complete.
  ASSERT_TRUE(content::WaitForLoadStop(instance->host().webui_contents()));

  content::WebContents* glic_contents = instance->host().webui_contents();
  auto* print_view_manager =
      printing::PrintViewManager::FromWebContents(glic_contents);
  print_view_manager->PrintNow(glic_contents->GetPrimaryMainFrame());

  // If we reached here without crashing, the test passes.
}
#endif

class GlicInstanceCoordinatorActorTaskTest
    : public GlicInstanceCoordinatorBrowserTest {
 public:
  GlicInstanceCoordinatorActorTaskTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kGlicActorPolicyControlExemption.name, "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorActorTaskTest,
                       ReloadCancelsActorTask) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  {
    auto info = glic::mojom::ConversationInfo::New();
    info->conversation_id = "conversation_id";
    instance->RegisterConversation(std::move(info), base::DoNothing());
  }

  // Wait for WebUI to be ready to ensure handler_info_ is set.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return instance->host().GetPrimaryWebUiState() ==
               glic::mojom::WebUiState::kReady &&
           instance->host().GetPrimaryWebClient();
  }));

  // Create a task to make it "actuating".
  ASSERT_OK(CreateActorTask(instance));
  EXPECT_TRUE(instance->IsActuating());

  // Reload the instance.
  instance->host().Reload();

  // Wait for StopTask to complete and verify that it is no longer actuating.
  EXPECT_TRUE(base::test::RunUntil([&]() { return !instance->IsActuating(); }));

  // verify that task can be created again.
  ASSERT_OK(CreateActorTask(instance));
  EXPECT_TRUE(instance->IsActuating());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorActorTaskTest,
                       OnTabAddedToTaskDoesNotShowPanelIfSuppressed) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  PreventDeletionOnClose(instance);
  instance->Close(EmbedderKey(tab), CloseOptions());
  ASSERT_OK(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kClosed));

  EXPECT_FALSE(instance->IsShowing());

  instance->SuppressShowOnNextTabAddedToTask(true);

  actor::TaskId task_id(123);
  instance->OnTabAddedToTask(task_id, tab->GetHandle());

  EXPECT_FALSE(instance->IsShowing());
  EXPECT_OK(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kClosed));

  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  instance->OnTabAddedToTask(task_id, tab2->GetHandle());

  EXPECT_OK(WaitForEmbedderActivationOrPeek(instance, tab2));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorActorTaskTest,
                       OnTabAddedToTaskShortcutsIfAlreadyBound) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  EXPECT_TRUE(instance->IsShowing());
  auto* embedder_before = instance->GetEmbedderForTab(tab);
  ASSERT_TRUE(embedder_before);

  actor::TaskId task_id(123);

  // Call OnTabAddedToTask for the already bound tab.
  instance->OnTabAddedToTask(task_id, tab->GetHandle());

  // It should still be showing and the embedder should not have changed.
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_EQ(embedder_before, instance->GetEmbedderForTab(tab));
}

class GlicInstanceCoordinatorHibernationTest
    : public GlicInstanceCoordinatorBrowserTest {
 public:
  GlicInstanceCoordinatorHibernationTest() {
    feature_list_.InitAndEnableFeatureWithParameters(kGlicMaxAwakeInstances,
                                                     {{"limit", "2"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorHibernationTest,
                       InstanceAwakeLimit) {
  // Create 4 instances when limit is 2.
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(auto* instance3, OpenGlicForActiveTab());
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(auto* instance4, OpenGlicForActiveTab());

  // We should have 4 instances, only the most recent 2 should be unhibernated.
  EXPECT_TRUE(instance1->IsHibernated());
  EXPECT_TRUE(instance2->IsHibernated());
  EXPECT_FALSE(instance3->IsHibernated());
  EXPECT_FALSE(instance4->IsHibernated());

  // Create a 5th instance. This should hibernate instance 3.
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(auto* instance5, OpenGlicForActiveTab());

  EXPECT_TRUE(instance1->IsHibernated());
  EXPECT_TRUE(instance2->IsHibernated());
  EXPECT_TRUE(instance3->IsHibernated());
  EXPECT_FALSE(instance4->IsHibernated());
  EXPECT_FALSE(instance5->IsHibernated());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ShowingInstancesAreNotHibernatedOnMemoryPressure) {
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());

  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());
  EXPECT_TRUE(instance2->IsShowing());

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  EXPECT_TRUE(instance1->IsHibernated());
  EXPECT_FALSE(instance2->IsHibernated());

  // Fire memory pressure again to verify instance2 is not hibernated.
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(instance2->IsHibernated());
}

class GlicInstanceCoordinatorNoWarmingTest
    : public GlicInstanceCoordinatorBrowserTest {
 public:
  GlicInstanceCoordinatorNoWarmingTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicWebContentsWarming,
                               {{"glic-web-contents-warming-delay", "60d"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorNoWarmingTest,
                       NoWarmingWhenDelayed) {
  GlicWebContentsWarmingPool& warming_pool =
      coordinator().GetWebContentsWarmingPoolForTesting();

  // Initially, there shouldn't be a warmed container.
  ASSERT_FALSE(warming_pool.HasWarmedContainerForTesting());

  // Activate an instance.
  ASSERT_OK(OpenGlicForActiveTab());

  // Give it a little time just in case of unexpected delayed tasks.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(50));
  run_loop.Run();

  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       StabilizationTaskSafeWithDestroyedWebContents) {
  // Create a tab.
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  // Create the task.
  auto task = std::make_unique<StabilizationTask>(web_contents);

  base::test::TestFuture<void> done_future;
  task->Start(done_future.GetCallback());

  // Destroy the tab.
  GetTabListInterface()->CloseTab(tab->GetHandle());

  // Wait for the task to complete. It should complete when the timer fires,
  // and it should not crash.
  EXPECT_TRUE(done_future.Wait());
}

class GlicInstanceCoordinatorActuationBrowserTest
    : public GlicInstanceCoordinatorBrowserTest {
 public:
  GlicInstanceCoordinatorActuationBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kGlicActorPolicyControlExemption.name, "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorActuationBrowserTest,
                       InvokeDelayedSuccessOnActuation) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(instance);

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();

  GlicInvokeOptions options(Target(*active_tab),
                            glic::mojom::InvocationSource::kOsButton);
  options.feature_mode = mojom::FeatureMode::kActuation;

  base::test::TestFuture<void> success_future;
  options.on_success = success_future.GetCallback();

  // Create a completion callback for the handler itself.
  base::test::TestFuture<void> handler_completion_future;
  auto handler = std::make_unique<GlicInvokeHandler>(
      *instance,
      GlicInvokeHandler::ResolvedTarget{
          GlicInvokeHandler::TabSurface{active_tab, false}},
      std::move(options), GlicInvokeWithAutoSubmitOptions(), std::nullopt,
      base::BindLambdaForTesting([&](GlicInstance*, GlicInvokeHandler*) {
        handler_completion_future.SetValue();
      }));

  // Subscribe to actuating changes.
  base::test::TestFuture<void> actuating_true_future;
  base::CallbackListSubscription subscription =
      instance->GetActorTaskManager()->AddActuatingChangedCallback(
          base::BindLambdaForTesting([&](bool actuating) {
            if (actuating) {
              actuating_true_future.SetValue();
            }
          }));

  handler->Invoke();

  // Create a task AFTER Invoke.
  ASSERT_OK_AND_ASSIGN(actor::TaskId task_id, CreateActorTask(instance));

  // Wait for IsActuating to be true.
  EXPECT_TRUE(actuating_true_future.Wait());

  // Verify that on_success is NOT called yet.
  EXPECT_FALSE(success_future.IsReady());

  // Stop the task and verify callback IS called.
  instance->GetActorTaskManager()->GetClientSessionForTesting()->StopActorTask(
      task_id.GetUnsafeValue(),
      glic::mojom::ActorTaskStopReason::kTaskComplete);

  EXPECT_TRUE(success_future.Wait());

  // Wait for the handler to complete.
  EXPECT_TRUE(handler_completion_future.Wait());
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       SaasUsageReportingOnOpen) {
  // Set the policy to include the GiC virtual domain.
  base::ListValue urls_list;
  urls_list.Append("gemini-in-chrome");
  GetProfile()->GetPrefs()->SetList(
      enterprise_reporting::kSaasUsageDomainUrlsForProfile,
      std::move(urls_list));

  // Opening Glic should trigger the reporting.
  ASSERT_OK(OpenGlicForActiveTab());

  const base::DictValue& report =
      GetProfile()->GetPrefs()->GetDict(enterprise_reporting::kSaasUsageReport);
  EXPECT_TRUE(report.contains("gemini-in-chrome"));
  EXPECT_EQ(report.FindDict("gemini-in-chrome")->FindInt("navigation_count"),
            1);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       SaasUsageReportingOnOpenAndDetach) {
  // Set the policy to include the GiC virtual domain.
  base::ListValue urls_list;
  urls_list.Append("gemini-in-chrome");
  GetProfile()->GetPrefs()->SetList(
      enterprise_reporting::kSaasUsageDomainUrlsForProfile,
      std::move(urls_list));

  // Opening Glic and detaching should trigger the reporting once.
  ASSERT_OK(OpenGlicForActiveTabAndDetach());

  const base::DictValue& report =
      GetProfile()->GetPrefs()->GetDict(enterprise_reporting::kSaasUsageReport);
  EXPECT_TRUE(report.contains("gemini-in-chrome"));
  EXPECT_EQ(report.FindDict("gemini-in-chrome")->FindInt("navigation_count"),
            1);
}
#endif

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       IsPanelShowingForBrowserSafeWithoutTabList) {
  testing::NiceMock<MockBrowserWindowInterface> mock_bwi;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_bwi, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  // Since user_data_host has no TabListInterface registered,
  // TabListInterface::From(&mock_bwi) will return nullptr.
  // IsPanelShowingForBrowser should return false and NOT crash.
  EXPECT_FALSE(coordinator().IsPanelShowingForBrowser(mock_bwi));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, HotkeyTriggersOpen) {
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 0u);

  // Simulate receiving the hotkey command.
  bool handled = coordinator().GetHotkeyManagerForTesting()->AcceleratorPressed(
      LocalHotkeyManager::Command::kOpenGlic);
  EXPECT_TRUE(handled);

  ASSERT_OK(WaitForGlicOpen());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       GetInvokeTargetCases) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // 1. Verify case when there is an active embedder.
  Target target = instance->GetInvokeTarget(Target::Surface());
  EXPECT_TRUE(std::holds_alternative<tabs::TabHandle>(target.surface));
  EXPECT_EQ(std::get<tabs::TabHandle>(target.surface), tab->GetHandle());
  // No conversation ID is registered yet, so it should fall back to the
  // instance ID.
  EXPECT_TRUE(std::holds_alternative<InstanceId>(target.conversation));
  EXPECT_EQ(std::get<InstanceId>(target.conversation), instance->id());

  // Register a conversation ID.
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = "test-conv-id";
  instance->RegisterConversation(std::move(info), base::DoNothing());

  // Get invoke target again and verify it now has the conversation ID.
  target = instance->GetInvokeTarget(Target::Surface());
  EXPECT_TRUE(std::holds_alternative<ConversationId>(target.conversation));
  EXPECT_EQ(std::get<ConversationId>(target.conversation).conversation_id,
            "test-conv-id");

  // Prevent deletion on close so the instance stays alive when closed.
  PreventDeletionOnClose(instance);

  // Close the embedder.
  instance->Close(EmbedderKey(tab), CloseOptions());
  ASSERT_OK(WaitForGlicClose(instance));

  // 2. Verify fallback case (no active embedder).
  EXPECT_FALSE(instance->HasActiveEmbedder());
  Target target_fallback =
      instance->GetInvokeTarget(Target::Surface(tab->GetHandle()));
  EXPECT_TRUE(std::holds_alternative<tabs::TabHandle>(target_fallback.surface));
  EXPECT_EQ(std::get<tabs::TabHandle>(target_fallback.surface),
            tab->GetHandle());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       InstanceKeptAliveByPinnedTabs) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // Pin the tab explicitly to keep the instance alive even if closed.
  instance->sharing_manager().PinTabs({tab->GetHandle()},
                                      GlicPinTrigger::kContextMenu);

  base::WeakPtr<GlicInstanceImpl> weak_instance = instance->GetWeakPtr();
  // Close the side panel (unbind the embedder).
  instance->UnbindEmbedder(EmbedderKey(tab));

  ASSERT_OK(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kClosed));
  EXPECT_TRUE(weak_instance);
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 1u);

  // Now unpin the tab.
  instance->sharing_manager().UnpinTabs({tab->GetHandle()});

  // Run until the instance is deleted asynchronously.
  ASSERT_OK(WaitForInstanceDeletion(weak_instance));
  EXPECT_EQ(coordinator().GetInstancesForTesting().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabRestoration_ReusesDyingInstanceBeforeDeletion) {
  // Add a new tab so we don't close the browser when we close the tab.
  auto* tab = CreateAndActivateTab(GURL("about:blank"));
  // Wait for contents to load to ensure that the tab will be eligible for
  // restoration.
  EXPECT_TRUE(content::WaitForLoadStop(tab->GetContents()));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto instance_id = instance->id();

  // Close the tab. This unbinds the tab and schedules the instance for deletion
  // asynchronously.
  GetTabListInterface()->CloseTab(tab->GetHandle());

  // Do NOT wait for instance deletion. The deletion task is now queued in the
  // message loop. Restore the tab immediately.
  GlicTestTabAddedWaiter waiter(GetProfile());
  RestoreMostRecentTab();
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  // Verify that the tab restoration finds and reuses the dying instance
  // (because it hasn't been deleted yet).
  ASSERT_OK_AND_ASSIGN(auto* restored_instance,
                       WaitForGlicInstanceBoundToTab(restored_tab));
  EXPECT_EQ(restored_instance, instance);

  // Now run the message loop. The queued deletion task will run.
  // Since the instance was reused (it now has the restored tab bound to it),
  // the double check in GlicInstanceImpl should prevent deletion!
  base::RunLoop().RunUntilIdle();

  // Verify the instance is STILL ALIVE and tracked by the coordinator!
  EXPECT_EQ(coordinator().GetInstanceImplFor(instance_id), instance);
}

}  // namespace glic
