// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: Avoid using RunTestSequence unless absolutely necessary. Simple
// synchronous operations should be called directly to keep tests easy to read
// and debug. When waiting is required, `base::test::RunUntil` is usually
// sufficient and simpler than a full `RunTestSequence`.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_close_options.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "content/public/browser/web_contents.h"
#else
#include "chrome/browser/ui/browser_commands.h"
#endif

namespace glic {

namespace {

// Waits for a tab to be added using GlicTabObserver.
class GlicTestTabAddedWaiter {
 public:
  explicit GlicTestTabAddedWaiter(Profile* profile) {
    tab_observer_ = GlicTabObserver::Create(
        profile, base::BindRepeating(&GlicTestTabAddedWaiter::OnTabEvent,
                                     base::Unretained(this)));
  }
  ~GlicTestTabAddedWaiter() = default;

  // Waits for a tab to be added and returns it.
  tabs::TabInterface* Wait() {
    run_loop_.Run();
    return new_tab_;
  }

 private:
  void OnTabEvent(const GlicTabEvent& event) {
    if (auto* creation = std::get_if<TabCreationEvent>(&event)) {
      new_tab_ = creation->new_tab;
      run_loop_.Quit();
    }
  }

  std::unique_ptr<GlicTabObserver> tab_observer_;
  base::RunLoop run_loop_;
  raw_ptr<tabs::TabInterface> new_tab_ = nullptr;
};

// Simulates a click on a link with the given modifiers.
// On Android, this uses a tap with modifiers, and injects a viewport meta tag
// to ensure coordinates are correct.
void SimulateLinkClick(tabs::TabInterface* tab, bool ctrl_key, bool shift_key) {
  content::WebContents* contents = tab->GetContents();
  std::string link_id = "simulator-link";
  std::string script = base::StringPrintf(
      R"(
        (() => {
          const meta = document.createElement('meta');
          meta.name = 'viewport';
          meta.content = 'width=device-width,minimum-scale=1';
          document.head.appendChild(meta);

          const a = document.createElement('a');
          a.id = '%s';
          a.href = 'about:blank';
          a.innerText = 'Click me';
          a.style.position = 'fixed';
          a.style.left = '0';
          a.style.top = '0';
          a.style.width = '100vw';
          a.style.height = '100vh';
          a.style.zIndex = '9999';
          document.body.appendChild(a);
        })();
      )",
      link_id.c_str());

  content::RenderFrameSubmissionObserver frame_observer(contents);

  EXPECT_TRUE(content::ExecJs(contents, script));

  // Wait for the next frame to ensure the element is visible to the compositor.
  // Without this wait, the click might happen too early and not trigger the
  // navigation.
  frame_observer.WaitForAnyFrameSubmission();

  int modifiers = 0;
  if (ctrl_key) {
#if BUILDFLAG(IS_MAC)
    modifiers |= blink::WebInputEvent::kMetaKey;
#else
    modifiers |= blink::WebInputEvent::kControlKey;
#endif
  }
  if (shift_key) {
    modifiers |= blink::WebInputEvent::kShiftKey;
  }

  gfx::Point point = gfx::ToFlooredPoint(
      content::GetCenterCoordinatesOfElementWithId(contents, link_id));

#if BUILDFLAG(IS_ANDROID)
  content::SimulateTapWithModifiersAt(contents, modifiers, point);
#else
  content::SimulateMouseClickAt(contents, modifiers,
                                blink::WebMouseEvent::Button::kLeft, point);
#endif
}

bool WaitForSidePanelState(tabs::TabInterface* tab,
                           GlicSidePanelCoordinator::State expected_state) {
  auto* side_panel_coordinator = GlicSidePanelCoordinator::GetForTab(tab);
  if (!side_panel_coordinator) {
    return false;
  }
  return base::test::RunUntil(
      [&]() { return side_panel_coordinator->state() == expected_state; });
}

void ActivateTab(tabs::TabInterface* tab) {
  CHECK(tab);
  tab->GetContents()->GetDelegate()->ActivateContents(tab->GetContents());
}

bool WaitForActiveEmbedderToMatchTab(GlicInstanceImpl* instance,
                                     tabs::TabInterface* tab) {
  CHECK(tab);
  CHECK(instance);
  return base::test::RunUntil(
      [&]() { return instance->GetActiveEmbedderTabForTesting() == tab; });
}

}  // namespace

class GlicInstanceCoordinatorBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicInstanceCoordinatorBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicMultiInstance,
                              features::kGlicDaisyChainNewTabs,
                              features::kGlicWebContentsWarming,
                              features::kGlicTabRestoration},
        /*disabled_features=*/{});
  }
  ~GlicInstanceCoordinatorBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // There are a number of APIs that aren't yet available on mobile android.
    // This should be removed once those are available.
    SKIP_TEST_FOR_NON_DESKTOP_ANDROID();
    GlicBrowserTestMixin::SetUpOnMainThread();
  }

  tabs::TabInterface* CreateUserInitiatedTab(const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
    auto* tab_list = GetTabListInterface();
    CHECK(tab_list) << "TabListInterface is null";
    auto* tab_model = static_cast<TabModel*>(tab_list);
    Profile* profile = GetProfile();
    CHECK(profile) << "Profile is null";

    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile));
    web_contents->GetController().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    tabs::TabInterface* new_tab =
        tab_model->CreateTab(nullptr, std::move(web_contents), -1,
                             TabModel::TabLaunchType::FROM_CHROME_UI, false);
    tab_model->ActivateTab(new_tab->GetHandle());
    return new_tab;
#else
    return CreateAndActivateTab(url);
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, InitialState) {
  EXPECT_EQ(coordinator().GetInstances().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ToggleCreatesInstance) {
  ToggleGlicForActiveTab();
  EXPECT_EQ(coordinator().GetInstances().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, CloseHidesInstance) {
  ToggleGlicForActiveTab();
  ASSERT_TRUE(WaitForGlicOpen());
  ToggleGlicForActiveTab();
  ASSERT_TRUE(WaitForGlicClose());
  for (auto* instance : coordinator().GetInstances()) {
    EXPECT_FALSE(instance->IsShowing());
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       CreateConversationForTabs) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  coordinator().CreateNewConversationForTabs({tab1, tab2});

  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1));
  EXPECT_EQ(coordinator().GetInstanceForTab(tab1),
            coordinator().GetInstanceForTab(tab2));
  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1)->IsShowing());
  EXPECT_FALSE(coordinator().GetInstances().empty());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       NewTabDaisyChaining) {
  GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, true);

  ASSERT_TRUE(OpenGlicForActiveTab());

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateUserInitiatedTab(GURL("about:blank"));

  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1));
  auto* tab2_instance = coordinator().GetInstanceImplForTab(tab2);
  EXPECT_TRUE(tab2_instance);
  EXPECT_NE(coordinator().GetInstanceForTab(tab1), tab2_instance);
  EXPECT_TRUE(WaitForActiveEmbedderToMatchTab(tab2_instance, tab2));

  GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);
  tabs::TabInterface* tab3 = CreateUserInitiatedTab(GURL("about:blank"));
  EXPECT_FALSE(coordinator().GetInstanceForTab(tab3));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ShowInstanceForTabs) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  auto* instance1 = OpenGlicForActiveTab();
  ASSERT_TRUE(instance1);
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  auto* instance2 = OpenGlicForActiveTab();
  ASSERT_TRUE(instance2);
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  EXPECT_NE(instance1, instance2);

  // Assign a conversation ID to instance2 so it can be targeted.
  // In production, this comes from the web client.
  const std::string kTargetConversationId = "conv_2";
  auto info = glic::mojom::ConversationInfo::New();
  info->conversation_id = kTargetConversationId;
  instance2->RegisterConversation(std::move(info), base::DoNothing());

  // Move tab1 to instance2's conversation.
  coordinator().ShowInstanceForTabs({tab1}, instance2->id());

  EXPECT_EQ(coordinator().GetInstanceForTab(tab1), instance2);
  EXPECT_EQ(coordinator().GetInstanceForTab(tab2), instance2);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabContentsDaisyChaining) {
  // SKIP_NEEDS_ANDROID_IMPL removed
  browser_activator()->SetMode(BrowserActivator::Mode::kFirst);

  auto* instance = OpenGlicForActiveTab();
  ASSERT_TRUE(instance);
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // Case 1: Ctrl+Click (New Tab)
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/false);
    tabs::TabInterface* tab2 = waiter.Wait();

    EXPECT_EQ(instance, coordinator().GetInstanceImplForTab(tab2));
    EXPECT_TRUE(tab1->IsActivated());

    // Verify side panel state for the background tab.
    EXPECT_TRUE(WaitForSidePanelState(
        tab2, GlicSidePanelCoordinator::State::kBackgrounded));

    // Activate the background tab and verify state becomes kShown.
    tab2->GetContents()->GetDelegate()->ActivateContents(tab2->GetContents());
    WaitForActiveEmbedderToMatchTab(instance, tab2);
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
    browser_activator()->SetActive(new_window);

    EXPECT_EQ(instance, coordinator().GetInstanceForTab(tab3));
    EXPECT_EQ(TabListInterface::From(new_window)->GetActiveTab(), tab3);
    EXPECT_TRUE(WaitForActiveEmbedderToMatchTab(instance, tab3));
    // Focus should be on the new window's page contents.
    EXPECT_FALSE(instance->HasFocus());
  }

  // Case 3: Ctrl+Shift+Click (Foreground Tab)
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    browser_activator()->SetActive(tab1->GetBrowserWindowInterface());
    GetTabListInterface()->ActivateTab(tab1->GetHandle());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/true);
    tabs::TabInterface* tab4 = waiter.Wait();

    EXPECT_EQ(instance, coordinator().GetInstanceForTab(tab4));
    EXPECT_EQ(GetTabListInterface()->GetActiveTab(), tab4);
    WaitForActiveEmbedderToMatchTab(instance, tab4);
    // Focus should be on the new foreground tab's page contents.
    EXPECT_FALSE(instance->HasFocus());
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabContentsDaisyChainingSuppressedWhenUnifiedFreShown) {
  // SKIP_NEEDS_ANDROID_IMPL removed

  auto* instance = OpenGlicForActiveTab();
  ASSERT_TRUE(instance);
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // Mock FRE opening
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(GetProfile());
  glic_service->fre_controller().SetIsShowingDialogForTesting(true);
  EXPECT_TRUE(glic_service->IsFreShowing());

  // Try to daisy chain via Page Contents
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/false);
    tabs::TabInterface* tab2 = waiter.Wait();

    GlicInstance* tab2_instance = coordinator().GetInstanceForTab(tab2);

    // Verify daisy chaining did not occur
    EXPECT_EQ(nullptr, tab2_instance);
  }
}

class GlicInstanceCoordinatorTrustFirstOnboardingArm1BrowserTest
    : public GlicInstanceCoordinatorBrowserTest {
 public:
  GlicInstanceCoordinatorTrustFirstOnboardingArm1BrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicTrustFirstOnboarding,
          {{features::kGlicTrustFirstOnboardingArmParam.name, "1"}}},
         {features::kGlicMultiInstance, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    GlicInstanceCoordinatorTrustFirstOnboardingArm1BrowserTest,
    TabContentsDaisyChainingNotSuppressedWhenTrustFirstArm1Shown) {
  // SKIP_NEEDS_ANDROID_IMPL removed

  // Open FRE.
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  auto* instance = OpenGlicForActiveTab();
  ASSERT_TRUE(instance);
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // Try to daisy chain via Page Contents
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/false);
    tabs::TabInterface* tab2 = waiter.Wait();

    GlicInstance* tab2_instance = coordinator().GetInstanceForTab(tab2);

    // Verify daisy chaining occurred.
    EXPECT_NE(nullptr, tab2_instance);
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       WebClientLinkClickDaisyChaining) {
  auto* instance = OpenGlicForActiveTab();
  ASSERT_TRUE(instance);

  // Case 1: Create Foreground Tab
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    instance->CreateTab(GURL("http://example.com"),
                        /*open_in_background=*/false,
                        /*window_id=*/std::nullopt, base::DoNothing());
    tabs::TabInterface* tab2 = waiter.Wait();

    EXPECT_EQ(instance, coordinator().GetInstanceForTab(tab2));
    EXPECT_TRUE(coordinator().GetInstanceForTab(tab2)->IsShowing());
    EXPECT_EQ(GetTabListInterface()->GetActiveTab(), tab2);
    // The glic embedder should not have focus when daisy chaining
    EXPECT_FALSE(instance->HasFocus());
  }

  // Case 2: Create Background Tab
  {
    GlicTestTabAddedWaiter waiter(GetProfile());
    instance->CreateTab(GURL("http://example.com"), /*open_in_background=*/true,
                        /*window_id=*/std::nullopt, base::DoNothing());
    tabs::TabInterface* tab3 = waiter.Wait();

    EXPECT_EQ(instance, coordinator().GetInstanceForTab(tab3));
    EXPECT_TRUE(coordinator().GetInstanceForTab(tab3)->IsShowing());
    // Active tab should still be previously active tab (tab2)
    EXPECT_NE(GetTabListInterface()->GetActiveTab(), tab3);

    EXPECT_TRUE(WaitForSidePanelState(
        tab3, GlicSidePanelCoordinator::State::kBackgrounded));

    ActivateTab(tab3);
    EXPECT_TRUE(
        WaitForSidePanelState(tab3, GlicSidePanelCoordinator::State::kShown));
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ActiveEmbedderFollowsActiveTab) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  coordinator().CreateNewConversationForTabs({tab1, tab2});

  // Switch back to tab 1.
  ActivateTab(tab1);
  EXPECT_TRUE(WaitForActiveEmbedderToMatchTab(
      coordinator().GetInstanceImplForTab(tab1), tab1));

  // Switch to tab 2.
  ActivateTab(tab2);
  EXPECT_TRUE(WaitForActiveEmbedderToMatchTab(
      coordinator().GetInstanceImplForTab(tab2), tab2));

  // Close tab 2 and verify tab 1 becomes the active embedder.
  // Note: Closing the active tab usually activates the nearest tab (tab 1).
  tab2->Close();
  EXPECT_TRUE(WaitForActiveEmbedderToMatchTab(
      coordinator().GetInstanceImplForTab(tab1), tab1));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       DeactivationWhenSwitchingToUnboundTab) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab3 = CreateAndActivateTab(GURL("about:blank"));

  // Bind tab1 and tab3 to the same instance.
  coordinator().CreateNewConversationForTabs({tab1, tab3});
  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_TRUE(instance);
  EXPECT_EQ(instance, coordinator().GetInstanceImplForTab(tab3));
  EXPECT_FALSE(coordinator().GetInstanceImplForTab(tab2));

  // Activate Tab 2 (Unbound) to establish history.
  ActivateTab(tab2);
  // Activate Tab 3 (Bound).
  ActivateTab(tab3);

  EXPECT_TRUE(WaitForActiveEmbedderToMatchTab(instance, tab3));

  base::test::TestFuture<GlicInstance*> future;
  auto subscription =
      coordinator().AddActiveInstanceChangedCallbackAndNotifyImmediately(
          future.GetRepeatingCallback());

  // Verify initial state notification.
  EXPECT_EQ(future.Take(), instance);

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
  auto* instance = OpenGlicForActiveTab();
  ASSERT_TRUE(instance);
  // Unpin the tab.
  instance->sharing_manager().UnpinTabs({tab->GetHandle()});
  EXPECT_FALSE(instance->sharing_manager().IsTabPinned(tab->GetHandle()));

  // Verify kContextMenu trigger explicitly pins the tab.
  coordinator().ShowInstanceForTabs({tab}, instance->id());
  EXPECT_TRUE(instance->sharing_manager().IsTabPinned(tab->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, TabRestoration) {
  SKIP_NEEDS_ANDROID_IMPL("Tab restore isn't supported on Android yet");
#if !BUILDFLAG(IS_ANDROID)
  // Add a new tab so we don't close the browser when we close the tab.
  auto* tab = CreateAndActivateTab(GURL("about:blank"));
  // Wait for contents to load to ensure that the tab will be eligible for
  // restoration.
  EXPECT_TRUE(content::WaitForLoadStop(tab->GetContents()));

  GlicInstanceImpl* instance = OpenGlicForActiveTab();
  ASSERT_TRUE(instance);
  auto instance_id = instance->id();

  // Simulates a user closing the active tab (Ctrl+W).
  // This ensures the generic "create historical tab" flag is set, which is
  // required for LiveTabContext to pick it up for session restore.
  chrome::CloseTab(browser());

  // Restore the tab.
  GlicTestTabAddedWaiter waiter(GetProfile());
  chrome::RestoreTab(browser());
  // Verify the restored tab is bound to the instance and the side panel is
  // open.
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  GlicInstance* restored_instance = nullptr;
  EXPECT_TRUE(base::test::RunUntil([&]() {
    restored_instance = coordinator().GetInstanceForTab(restored_tab);
    return restored_instance != nullptr;
  }));

  EXPECT_TRUE(restored_instance->IsShowing());
  EXPECT_EQ(restored_instance->id(), instance_id);
#endif
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabRestoration_PinnedButNotBound) {
  SKIP_NEEDS_ANDROID_IMPL("Tab restore isn't supported on Android yet");
#if !BUILDFLAG(IS_ANDROID)
  // Tab 1: Keep Instance 1 alive.
  CreateAndActivateTab(GURL("about:blank"));
  GlicInstanceImpl* instance1 = OpenGlicForActiveTab();
  ASSERT_TRUE(instance1);
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

  // Close Tab 2.
  chrome::CloseTab(browser());

  // Restore Tab 2.
  GlicTestTabAddedWaiter waiter(GetProfile());
  chrome::RestoreTab(browser());
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  auto* restored_instance2 = WaitForGlicInstanceBoundToTab(restored_tab);
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
  EXPECT_TRUE(WaitForSidePanelState(restored_tab,
                                    GlicSidePanelCoordinator::State::kShown));
#endif
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabRestoration_RestoreToExistingInstance) {
  SKIP_NEEDS_ANDROID_IMPL("Tab restore isn't supported on Android yet");
#if !BUILDFLAG(IS_ANDROID)
  // Tab 1: Keep the instance alive.
  CreateAndActivateTab(GURL("about:blank"));
  GlicInstanceImpl* instance = OpenGlicForActiveTab();
  ASSERT_TRUE(instance);
  auto instance_id = instance->id();

  // Tab 2: The one we will close and restore.
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  // Wait for contents to load to ensure that the tab will be eligible for
  // restoration.
  EXPECT_TRUE(content::WaitForLoadStop(tab2->GetContents()));
  coordinator().ShowInstanceForTabs({tab2}, instance_id);
  ASSERT_EQ(coordinator().GetInstanceForTab(tab2), instance);

  // Close Tab 2.
  chrome::CloseTab(browser());

  // The instance should still exist because Tab 1 keeps it alive.
  ASSERT_EQ(coordinator().GetInstanceImplFor(instance_id), instance);

  // Restore Tab 2.
  GlicTestTabAddedWaiter waiter(GetProfile());
  chrome::RestoreTab(browser());
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  auto* bound_instance = WaitForGlicInstanceBoundToTab(restored_tab);

  // Should have reused the existing instance.
  EXPECT_EQ(bound_instance, instance);
  EXPECT_TRUE(WaitForSidePanelState(restored_tab,
                                    GlicSidePanelCoordinator::State::kShown));
#endif
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       TabRestoration_SidePanelClosed) {
  SKIP_NEEDS_ANDROID_IMPL("Tab restore isn't supported on Android yet");
#if !BUILDFLAG(IS_ANDROID)
  // Add a new tab so we don't close the browser when we close the tab.
  auto* tab = CreateAndActivateTab(GURL("about:blank"));
  // Wait for contents to load to ensure that the tab will be eligible for
  // restoration.
  EXPECT_TRUE(content::WaitForLoadStop(tab->GetContents()));

  GlicInstanceImpl* instance = OpenGlicForActiveTab();
  ASSERT_TRUE(instance);

  // Register a conversation ID to prevent the instance from being deleted
  // when the side panel is closed.
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = "test_conversation_id";
  instance->RegisterConversation(std::move(info), base::DoNothing());

  // Close the side panel for this tab.
  auto original_instance_id = instance->id();
  instance->Close(EmbedderKey(tab), CloseOptions());
  ASSERT_TRUE(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kClosed));

  // Close the tab.
  chrome::CloseTab(browser());

  // Restore the tab.
  GlicTestTabAddedWaiter waiter(GetProfile());
  chrome::RestoreTab(browser());
  tabs::TabInterface* restored_tab = waiter.Wait();
  ASSERT_TRUE(restored_tab);

  auto* bound_instance = WaitForGlicInstanceBoundToTab(restored_tab);

  // Since it was closed (unbound) when we closed the tab, it should be restored
  // as unbound.
  EXPECT_EQ(original_instance_id, bound_instance->id());
  EXPECT_TRUE(WaitForSidePanelState(restored_tab,
                                    GlicSidePanelCoordinator::State::kClosed));
#endif
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
  auto* instance1 = OpenGlicForActiveTab();
  ASSERT_TRUE(instance1);
  CreateAndActivateTab(GURL("about:blank"));
  auto* instance2 = OpenGlicForActiveTab();
  ASSERT_TRUE(instance2);
  CreateAndActivateTab(GURL("about:blank"));
  auto* instance3 = OpenGlicForActiveTab();
  ASSERT_TRUE(instance3);
  CreateAndActivateTab(GURL("about:blank"));
  auto* instance4 = OpenGlicForActiveTab();
  ASSERT_TRUE(instance4);

  // We should have 4 instances, only the most recent 2 should be unhibernated.
  EXPECT_TRUE(instance1->IsHibernated());
  EXPECT_TRUE(instance2->IsHibernated());
  EXPECT_FALSE(instance3->IsHibernated());
  EXPECT_FALSE(instance4->IsHibernated());

  // Create a 5th instance. This should hibernate instance 3.
  CreateAndActivateTab(GURL("about:blank"));
  auto* instance5 = OpenGlicForActiveTab();
  ASSERT_TRUE(instance5);

  EXPECT_TRUE(instance1->IsHibernated());
  EXPECT_TRUE(instance2->IsHibernated());
  EXPECT_TRUE(instance3->IsHibernated());
  EXPECT_FALSE(instance4->IsHibernated());
  EXPECT_FALSE(instance5->IsHibernated());
}

class GlicInstanceCoordinatorToggleWithConversationTest
    : public GlicInstanceCoordinatorBrowserTest {
 public:
  GlicInstanceCoordinatorToggleWithConversationTest() {
    feature_list_.InitAndEnableFeature(features::kGlicWebContinuity);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorToggleWithConversationTest,
                       ToggleWithConversationId) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  const std::string cid1 = "conv_1";
  const std::string cid2 = "conv_2";

  // Toggle with cid1.
  coordinator().Toggle(tab1->GetBrowserWindowInterface(),
                       /*prevent_close=*/false,
                       mojom::InvocationSource::kTopChromeButton,
                       /*deprecated_prompt_suggestion=*/std::nullopt,
                       /*deprecated_auto_send=*/false, cid1);

  auto* instance1 = WaitForGlicOpen(tab1);
  ASSERT_TRUE(instance1);
  EXPECT_EQ(instance1->conversation_id(), cid1);
  EXPECT_EQ(coordinator().GetInstances().size(), 1u);

  // Toggle again with SAME cid1 on a second tab. Should reuse instance1.
  auto* tab2 = CreateAndActivateTab(GURL("about:blank"));
  coordinator().Toggle(tab2->GetBrowserWindowInterface(),
                       /*prevent_close=*/false,
                       mojom::InvocationSource::kTopChromeButton,
                       /*deprecated_prompt_suggestion=*/std::nullopt,
                       /*deprecated_auto_send=*/false, cid1);

  ASSERT_TRUE(WaitForActiveEmbedderToMatchTab(instance1, tab2));
  EXPECT_EQ(coordinator().GetInstances().size(), 1u);

  // Toggle with DIFFERENT cid2 on a third tab. Should create a new instance.
  auto* tab3 = CreateAndActivateTab(GURL("about:blank"));
  coordinator().Toggle(tab3->GetBrowserWindowInterface(),
                       /*prevent_close=*/false,
                       mojom::InvocationSource::kTopChromeButton,
                       /*deprecated_prompt_suggestion=*/std::nullopt,
                       /*deprecated_auto_send=*/false, cid2);

  auto* instance2 = WaitForGlicInstanceBoundToTab(tab3);
  ASSERT_TRUE(instance2);
  EXPECT_EQ(instance2->conversation_id(), cid2);
  EXPECT_EQ(coordinator().GetInstances().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       InvokeWithInvalidTab) {
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(/*tab=*/nullptr, std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidTab);
}

}  // namespace glic
