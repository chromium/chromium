// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_observer.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point_conversions.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#endif

#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace {
// Simulates a click on a link with the given modifiers.
// On Android, this uses a tap with modifiers, and injects a viewport meta tag
// to ensure coordinates are correct.

}  // namespace

// Test versions of event structs that use WeakPtrs.
// The GlicTabEvent structs guarantee pointer validity only for the duration of
// the callback. Since this test collector stores events for later verification,
// we violate that lifetime guarantee. We must use WeakPtrs to safely handle
// cases where tabs are destroyed before the test verifies the events.
struct TestTabCreationEvent {
  base::WeakPtr<tabs::TabInterface> new_tab;
  base::WeakPtr<tabs::TabInterface> old_tab;
  base::WeakPtr<tabs::TabInterface> opener;
  TabCreationType creation_type = TabCreationType::kUnknown;
};

struct TestTabActivationEvent {
  base::WeakPtr<tabs::TabInterface> new_active_tab;
  base::WeakPtr<tabs::TabInterface> old_active_tab;
};

using TestGlicTabEvent = std::
    variant<TestTabCreationEvent, TabMutationEvent, TestTabActivationEvent>;

TestGlicTabEvent ConvertToTestEvent(const GlicTabEvent& event) {
  if (const auto* c = std::get_if<TabCreationEvent>(&event)) {
    return TestTabCreationEvent{c->new_tab ? c->new_tab->GetWeakPtr() : nullptr,
                                c->old_tab ? c->old_tab->GetWeakPtr() : nullptr,
                                c->opener ? c->opener->GetWeakPtr() : nullptr,
                                c->creation_type};
  } else if (const auto* a = std::get_if<TabActivationEvent>(&event)) {
    return TestTabActivationEvent{
        a->new_active_tab ? a->new_active_tab->GetWeakPtr() : nullptr,
        a->old_active_tab ? a->old_active_tab->GetWeakPtr() : nullptr};
  } else if (std::holds_alternative<TabMutationEvent>(event)) {
    return TabMutationEvent{};
  }
  return TabMutationEvent{};
}

class GlicTabEventCollector {
 public:
  explicit GlicTabEventCollector(Profile* profile) {
    observer_ = GlicTabObserver::Create(
        profile, base::BindRepeating(&GlicTabEventCollector::OnEvent,
                                     // Unretained is safe because observer_ is
                                     // destroyed before this object.
                                     base::Unretained(this)));
  }

  void OnEvent(const GlicTabEvent& event) {
    events_.push_back(ConvertToTestEvent(event));
    if (predicate_ && predicate_.Run(events_.back())) {
      condition_met_signal_.SetValue();
    }
  }

  void WaitForEvent(
      base::RepeatingCallback<bool(const TestGlicTabEvent&)> predicate) {
    // Check if event already occurred
    for (const auto& event : events_) {
      if (predicate.Run(event)) {
        return;
      }
    }

    predicate_ = predicate;
    condition_met_signal_.Clear();
    EXPECT_TRUE(condition_met_signal_.Wait());
    predicate_.Reset();
  }

  const TestTabCreationEvent* WaitForCreation() {
    WaitForEvent(base::BindRepeating([](const TestGlicTabEvent& event) {
      return std::holds_alternative<TestTabCreationEvent>(event);
    }));
    for (auto& event : base::Reversed(events_)) {
      if (const auto* c = std::get_if<TestTabCreationEvent>(&event)) {
        return c;
      }
    }
    return nullptr;
  }

  const TestTabActivationEvent* WaitForActivation() {
    WaitForEvent(base::BindRepeating([](const TestGlicTabEvent& event) {
      return std::holds_alternative<TestTabActivationEvent>(event);
    }));
    for (auto& event : base::Reversed(events_)) {
      if (const auto* a = std::get_if<TestTabActivationEvent>(&event)) {
        return a;
      }
    }
    return nullptr;
  }

  void WaitForMutation() {
    WaitForEvent(base::BindRepeating([](const TestGlicTabEvent& event) {
      return std::holds_alternative<TabMutationEvent>(event);
    }));
  }

  const std::vector<TestGlicTabEvent>& events() const { return events_; }

  void ClearEvents() { events_.clear(); }

 private:
  std::unique_ptr<GlicTabObserver> observer_;
  std::vector<TestGlicTabEvent> events_;
  base::RepeatingCallback<bool(const TestGlicTabEvent&)> predicate_;
  base::test::TestFuture<void> condition_met_signal_;
};

class GlicTabObserverBrowserTest : public PlatformBrowserTest {
 public:
  ~GlicTabObserverBrowserTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);
#if BUILDFLAG(IS_ANDROID)
    command_line->AppendSwitch(switches::kForceDesktopAndroid);
#endif
  }

 protected:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(GetProfile());
  }

#if !BUILDFLAG(IS_ANDROID)
  TabListInterface* CreateIncognitoTabList() {
    Profile* incognito_profile =
        GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    BrowserWindowCreateParams create_params(BrowserWindowInterface::TYPE_NORMAL,
                                            *incognito_profile, false);
    base::test::TestFuture<BrowserWindowInterface*> future;
    CreateBrowserWindow(std::move(create_params), future.GetCallback());
    return TabListInterface::From(future.Get());
  }
#endif

  tabs::TabInterface* CreateTab() {
    tabs::TabInterface* new_tab =
        GetTabListInterface()->OpenTab(GURL("about:blank"), -1);
    GetTabListInterface()->ActivateTab(new_tab->GetHandle());
    return new_tab;
  }

  void NavigateTab(tabs::TabInterface* tab, const GURL& url) {
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::CURRENT_TAB,
                                  ui::PAGE_TRANSITION_TYPED,
                                  /*is_renderer_initiated=*/false);
    tab->GetContents()->OpenURL(params, base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabCreation) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/477918431): Flaky on non-desktop Android.
  if (!base::android::device_info::is_desktop()) {
    GTEST_SKIP() << "Skipping on non-desktop Android";
  }
#endif
  GlicTabEventCollector collector(GetProfile());

  // Initial tab verification
  tabs::TabInterface* initial_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  // Open Tab 2
  tabs::TabInterface* second_tab = CreateTab();
  const TestTabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_NE(creation->new_tab, nullptr);
  EXPECT_EQ(creation->old_tab.get(), initial_tab);
  EXPECT_EQ(creation->new_tab.get(), second_tab);

  // Clear events to ensure we wait for the NEXT creation.
  collector.ClearEvents();

  // Open Tab 3
  tabs::TabInterface* third_tab = CreateTab();
  creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_NE(creation->new_tab, nullptr);
  EXPECT_EQ(creation->old_tab.get(), second_tab);
  EXPECT_EQ(creation->new_tab.get(), third_tab);
}

// TODO: See if we can create a multi-window test on android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       ObservesTabCreationInNewWindow) {
  GlicTabEventCollector collector(GetProfile());

  BrowserWindowCreateParams create_params(BrowserWindowInterface::TYPE_NORMAL,
                                          *GetProfile(), false);
  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* new_window = future.Get();
  ASSERT_TRUE(new_window);

  TabListInterface* new_tab_list = TabListInterface::From(new_window);
  ASSERT_TRUE(new_tab_list);
  new_tab_list->OpenTab(GURL("about:blank"), -1);

  const TestTabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_NE(creation->new_tab, nullptr);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       DoesNotObserveTabCreationInDifferentProfile) {
  TabListInterface* incognito_tab_list = CreateIncognitoTabList();
  GlicTabEventCollector collector(GetProfile());

  // Create tab in incognito. Should NOT trigger event.
  tabs::TabInterface* incognito_tab =
      incognito_tab_list->OpenTab(GURL("about:blank"), -1);

  // Create tab in regular profile. Should trigger event.
  tabs::TabInterface* regular_tab = CreateTab();

  const TestTabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_EQ(creation->creation_type, TabCreationType::kUserInitiated);
  EXPECT_EQ(creation->new_tab.get(), regular_tab);

  // Verify none of the events were for the incognito browser.
  for (const auto& event : collector.events()) {
    if (const auto* c = std::get_if<TestTabCreationEvent>(&event)) {
      EXPECT_NE(c->new_tab.get(), incognito_tab);
    }
  }
}
#endif

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabMutation) {
  GlicTabEventCollector collector(GetProfile());

  // Create a tab so we can close it.
  tabs::TabInterface* tab_to_close = CreateTab();
  collector.WaitForCreation();

  // Create another tab to keep the browser alive.
  CreateTab();
  collector.WaitForCreation();

  collector.ClearEvents();

  // Close the tab. This should trigger a TabMutationEvent.
  GetTabListInterface()->CloseTab(tab_to_close->GetHandle());

  collector.WaitForMutation();

  // If we got here, we successfully observed a mutation.
  bool found_mutation = false;
  for (const auto& event : collector.events()) {
    if (std::holds_alternative<TabMutationEvent>(event)) {
      found_mutation = true;
      break;
    }
  }
  EXPECT_TRUE(found_mutation);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabMove) {
  GlicTabEventCollector collector(GetProfile());

  // Create two tabs so we can move one.
  CreateTab();
  tabs::TabInterface* tab_to_move = CreateTab();
  // tabs: [0 (initial), 1, 2]

  // Move tab at index 2 to index 0.
  GetTabListInterface()->MoveTab(tab_to_move->GetHandle(), 0);

  collector.WaitForMutation();
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabStripMerge) {
  Browser* browser2 = CreateBrowser(GetProfile());
  GlicTabEventCollector collector(GetProfile());

  std::unique_ptr<content::WebContents> contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  browser()->tab_strip_model()->InsertWebContentsAt(0, std::move(contents),
                                                    AddTabTypes::ADD_ACTIVE);

  // We expect both insertion and likely some mutations from the detach/insert.
  collector.WaitForCreation();

  bool found_removal = false;
  bool found_insertion = false;

  for (const auto& event : collector.events()) {
    if (std::holds_alternative<TabMutationEvent>(event)) {
      found_removal = true;
    } else if (const auto* creation =
                   std::get_if<TestTabCreationEvent>(&event)) {
      if (creation->new_tab) {
        found_insertion = true;
      }
    }
  }

  EXPECT_TRUE(found_removal);
  EXPECT_TRUE(found_insertion);
}
#endif

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabNavigation) {
  GlicTabEventCollector collector(GetProfile());

  // Create and activate a tab to ensure we have a valid active tab to navigate.
  tabs::TabInterface* tab = CreateTab();
  collector.WaitForCreation();
  collector.ClearEvents();

  // Navigate. This should trigger updates (e.g. loading state change).
  NavigateTab(tab, GURL("about:blank"));

  // We expect *some* mutation event (loading state, etc.)
  collector.WaitForMutation();
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabActivation) {
  GlicTabEventCollector collector(GetProfile());
  tabs::TabInterface* initial_tab = GetTabListInterface()->GetActiveTab();

  // Create a tab so we can switch to it.
  tabs::TabInterface* second_tab = CreateTab();
  const TestTabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_EQ(creation->new_tab.get(), second_tab);
  collector.ClearEvents();

  // Switch back to the first tab.
  GetTabListInterface()->ActivateTab(initial_tab->GetHandle());

  const TestTabActivationEvent* activation = collector.WaitForActivation();
  ASSERT_TRUE(activation);
  EXPECT_EQ(activation->new_active_tab.get(), initial_tab);
  EXPECT_EQ(activation->old_active_tab.get(), second_tab);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, LinkClickTracking) {
  GlicTabEventCollector collector(GetProfile());

  // 1. Get initial tab
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(first_tab);

  // 2. Simulate opening a link in a new tab
  content::OpenURLParams params(GURL("about:blank"), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);
  first_tab->GetContents()->OpenURL(params, base::DoNothing());

  const TestTabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  ASSERT_TRUE(creation->new_tab);

  EXPECT_EQ(creation->creation_type, TabCreationType::kFromLink);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, LinkClickNewWindowTracking) {
  GlicTabEventCollector collector(GetProfile());

  // 1. Get initial tab
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(first_tab);

  // 2. Simulate opening a link in a NEW WINDOW (Shift+Click)
  content::OpenURLParams params(GURL("about:blank"), content::Referrer(),
                                WindowOpenDisposition::NEW_WINDOW,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);
  params.source_render_process_id = first_tab->GetContents()
                                        ->GetPrimaryMainFrame()
                                        ->GetProcess()
                                        ->GetDeprecatedID();
  params.source_render_frame_id =
      first_tab->GetContents()->GetPrimaryMainFrame()->GetRoutingID();
  params.has_rel_opener = true;
  first_tab->GetContents()->OpenURL(params, base::DoNothing());

  const TestTabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  ASSERT_TRUE(creation->new_tab);

// GetBrowserWindowInterface() always returns nullptr on non-desktop Android.
// And android browser tests don't allow multiple windows, so this test will
// open the tab a new tab in the same window.
#if !BUILDFLAG(IS_ANDROID)
  // Verify that it opened in a new window
  EXPECT_NE(creation->new_tab->GetBrowserWindowInterface(),
            first_tab->GetBrowserWindowInterface());
#endif

  // Verify the opener is preserved
  EXPECT_EQ(creation->opener.get(), first_tab);
  EXPECT_EQ(creation->creation_type, TabCreationType::kFromLink);
}
