// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: Avoid using RunTestSequence unless absolutely necessary. Simple
// synchronous operations should be called directly to keep tests easy to read
// and debug. When waiting is required, `base::test::RunUntil` is usually
// sufficient and simpler than a full `RunTestSequence`.

#include "base/task/current_thread.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_close_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

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

void SimulateLinkClick(tabs::TabInterface* tab, bool ctrl_key, bool shift_key) {
  content::WebContents* contents = tab->GetContents();
  std::string link_id = "simulator-link";
  std::string script = base::StringPrintf(
      R"(
        (() => {
          const a = document.createElement('a');
          a.id = '%s';
          a.href = 'about:blank';
          a.target = '_blank';
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
  EXPECT_TRUE(content::ExecJs(contents, script));

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

  content::SimulateMouseClickAt(
      contents, modifiers, blink::WebMouseEvent::Button::kLeft,
      gfx::ToFlooredPoint(
          content::GetCenterCoordinatesOfElementWithId(contents, link_id)));
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

class GlicInstanceCoordinatorBrowserTest : public NonInteractiveGlicTest {
 public:
  GlicInstanceCoordinatorBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicMultiInstance,
                              features::kGlicDaisyChainNewTabs,
                              features::kGlicWebContentsWarming},
        /*disabled_features=*/{});
  }
  ~GlicInstanceCoordinatorBrowserTest() override = default;

  GlicInstanceCoordinatorImpl& coordinator() {
    return static_cast<GlicInstanceCoordinatorImpl&>(window_controller());
  }

  void ToggleGlic(bool prevent_close = false) {
    coordinator().Toggle(browser(), prevent_close,
                         mojom::InvocationSource::kOsButton,
                         /*prompt_suggestion=*/std::nullopt);
  }

  tabs::TabInterface* AddTab() {
    content::WebContents* contents = chrome::AddAndReturnTabAt(
        browser(), GURL("about:blank"), /*tab_index=*/-1, /*foreground=*/true);
    return tabs::TabInterface::GetFromContents(contents);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, CoordinatorExists) {
  EXPECT_TRUE(&coordinator());
  EXPECT_EQ(&coordinator(),
            &GlicKeyedService::Get(browser()->profile())->window_controller());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, InitialState) {
  EXPECT_EQ(coordinator().GetInstances().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ToggleCreatesInstance) {
  ToggleGlic();
  EXPECT_EQ(coordinator().GetInstances().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, CloseHidesInstance) {
  RunTestSequence(Do([this]() { ToggleGlic(); }), WaitForGlicOpen(),
                  Do([this]() { ToggleGlic(); }), WaitForGlicClose());
  for (auto* instance : coordinator().GetInstances()) {
    EXPECT_FALSE(instance->IsShowing());
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       CreateConversationForTabs) {
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  tabs::TabInterface* tab2 = AddTab();

  coordinator().CreateNewConversationForTabs({tab1, tab2});

  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1));
  EXPECT_EQ(coordinator().GetInstanceForTab(tab1),
            coordinator().GetInstanceForTab(tab2));
  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1)->IsShowing());
  EXPECT_FALSE(coordinator().GetInstances().empty());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       NewTabDaisyChaining) {
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, true);

  ToggleGlic();

  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  tabs::TabInterface* tab2 = AddTab();

  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1));
  EXPECT_TRUE(coordinator().GetInstanceForTab(tab2));
  EXPECT_NE(coordinator().GetInstanceForTab(tab1),
            coordinator().GetInstanceForTab(tab2));
  EXPECT_TRUE(coordinator().GetInstanceForTab(tab2)->IsShowing());

  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);
  tabs::TabInterface* tab3 = AddTab();
  EXPECT_FALSE(coordinator().GetInstanceForTab(tab3));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       MoveTabsToConversation) {
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  ToggleGlic();
  tabs::TabInterface* tab2 = AddTab();
  ToggleGlic();
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  GlicInstanceImpl* instance1 = coordinator().GetInstanceImplForTab(tab1);
  EXPECT_TRUE(instance1);
  GlicInstanceImpl* instance2 = coordinator().GetInstanceImplForTab(tab2);
  EXPECT_TRUE(instance2);
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
  ToggleGlic();
  RunTestSequence(WaitForGlicOpen());
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();

  // Allow multiple browsers for the Shift+Click test case.
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);

  // Case 1: Ctrl+Click (New Tab)
  {
    GlicTestTabAddedWaiter waiter(browser()->profile());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/false);
    tabs::TabInterface* tab2 = waiter.Wait();

    GlicInstance* tab2_instance = coordinator().GetInstanceForTab(tab2);

    EXPECT_EQ(coordinator().GetInstanceForTab(tab1), tab2_instance);
    EXPECT_TRUE(tab2_instance->IsShowing());
    EXPECT_EQ(tab1->GetBrowserWindowInterface()->GetActiveTabInterface(), tab1);

    // Verify side panel state for the background tab.
    EXPECT_TRUE(WaitForSidePanelState(
        tab2, GlicSidePanelCoordinator::State::kBackgrounded));

    // Activate the background tab and verify state becomes kShown.
    tab2->GetContents()->GetDelegate()->ActivateContents(tab2->GetContents());
    EXPECT_TRUE(
        WaitForSidePanelState(tab2, GlicSidePanelCoordinator::State::kShown));
  }

  // Case 2: Shift+Click (New Window)
  {
    GlicTestTabAddedWaiter waiter(browser()->profile());
    SimulateLinkClick(tab1, /*ctrl_key=*/false, /*shift_key=*/true);

    tabs::TabInterface* tab3 = waiter.Wait();

    GlicInstance* tab3_instance = coordinator().GetInstanceForTab(tab3);

    EXPECT_EQ(coordinator().GetInstanceForTab(tab1), tab3_instance);
    EXPECT_TRUE(tab3_instance->IsShowing());
    EXPECT_EQ(tab3->GetBrowserWindowInterface()->GetActiveTabInterface(), tab3);
  }

  // Case 3: Ctrl+Shift+Click (Foreground Tab)
  {
    GlicTestTabAddedWaiter waiter(browser()->profile());
    SimulateLinkClick(tab1, /*ctrl_key=*/true, /*shift_key=*/true);
    tabs::TabInterface* tab4 = waiter.Wait();

    GlicInstance* tab4_instance = coordinator().GetInstanceForTab(tab4);

    EXPECT_EQ(coordinator().GetInstanceForTab(tab1), tab4_instance);
    EXPECT_TRUE(tab4_instance->IsShowing());
    EXPECT_EQ(tab1->GetBrowserWindowInterface()->GetActiveTabInterface(), tab4);
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       WebClientLinkClickDaisyChaining) {
  ToggleGlic();
  RunTestSequence(WaitForGlicOpen());
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_TRUE(instance);

  // Case 1: Create Foreground Tab
  {
    GlicTestTabAddedWaiter waiter(browser()->profile());
    instance->CreateTab(GURL("http://example.com"),
                        /*open_in_background=*/false,
                        /*window_id=*/std::nullopt, base::DoNothing());
    tabs::TabInterface* tab2 = waiter.Wait();

    EXPECT_EQ(instance, coordinator().GetInstanceForTab(tab2));
    EXPECT_TRUE(coordinator().GetInstanceForTab(tab2)->IsShowing());
    EXPECT_EQ(tab1->GetBrowserWindowInterface()->GetActiveTabInterface(), tab2);
  }

  // Case 2: Create Background Tab
  {
    GlicTestTabAddedWaiter waiter(browser()->profile());
    instance->CreateTab(GURL("http://example.com"), /*open_in_background=*/true,
                        /*window_id=*/std::nullopt, base::DoNothing());
    tabs::TabInterface* tab3 = waiter.Wait();

    EXPECT_EQ(instance, coordinator().GetInstanceForTab(tab3));
    EXPECT_TRUE(coordinator().GetInstanceForTab(tab3)->IsShowing());
    // Active tab should still be previously active tab (tab2)
    EXPECT_NE(tab1->GetBrowserWindowInterface()->GetActiveTabInterface(), tab3);

    EXPECT_TRUE(WaitForSidePanelState(
        tab3, GlicSidePanelCoordinator::State::kBackgrounded));

    ActivateTab(tab3);
    EXPECT_TRUE(
        WaitForSidePanelState(tab3, GlicSidePanelCoordinator::State::kShown));
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ActiveEmbedderFollowsActiveTab) {
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  tabs::TabInterface* tab2 = AddTab();

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
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  tabs::TabInterface* tab2 = AddTab();
  tabs::TabInterface* tab3 = AddTab();

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

}  // namespace glic
