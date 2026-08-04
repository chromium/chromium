// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_observer.h"

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/dom/dom_code.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "build/android_buildflags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#endif

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

  [[nodiscard]] base::expected<TestTabCreationEvent, std::string>
  WaitForCreation() {
    WaitForEvent(base::BindRepeating([](const TestGlicTabEvent& event) {
      return std::holds_alternative<TestTabCreationEvent>(event);
    }));
    for (auto& event : base::Reversed(events_)) {
      if (const auto* c = std::get_if<TestTabCreationEvent>(&event)) {
        return *c;
      }
    }
    return base::unexpected(
        "Tab creation event not found in collector history.");
  }

  [[nodiscard]] base::expected<TestTabActivationEvent, std::string>
  WaitForActivation() {
    WaitForEvent(base::BindRepeating([](const TestGlicTabEvent& event) {
      return std::holds_alternative<TestTabActivationEvent>(event);
    }));
    for (auto& event : base::Reversed(events_)) {
      if (const auto* a = std::get_if<TestTabActivationEvent>(&event)) {
        return *a;
      }
    }
    return base::unexpected(
        "Tab activation event not found in collector history.");
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

class GlicTabObserverBrowserTest
    : public glic::GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicTabObserverBrowserTest() = default;

  ~GlicTabObserverBrowserTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);
#if BUILDFLAG(IS_DESKTOP_ANDROID)
    command_line->AppendSwitch(switches::kForceDesktopAndroid);
#endif
#if BUILDFLAG(IS_ANDROID)
    command_line->AppendSwitch("disable-fre");
#endif
  }

  void SetUp() override {
    if (!glic::GlicEnabling::IsOsVersionSupported()) {
      GTEST_SKIP() << "OS version not supported by Glic";
    }
    PlatformBrowserTest::SetUp();
  }

 protected:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(GetProfile());
  }

  BrowserWindowInterface* CreateNewWindowWithTab() {
    return CreateAdditionalBrowserWindow();
  }

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
  TabListInterface* CreateIncognitoTabList() {
    Profile* incognito_profile =
        GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    BrowserWindowInterface* window =
        glic::CreateBrowserWindow(incognito_profile);
    return TabListInterface::From(window);
  }
#endif

  tabs::TabInterface* CreateTab(TabListInterface* tab_list = nullptr) {
    if (!tab_list) {
      tab_list = GetTabListInterface();
    }
    content::TestNavigationObserver navigation_observer(GURL("about:blank"));
    navigation_observer.StartWatchingNewWebContents();
    tabs::TabInterface* new_tab = tab_list->OpenTab(GURL("about:blank"), -1);
    tab_list->ActivateTab(new_tab->GetHandle());
    navigation_observer.Wait();
    return new_tab;
  }

  void NavigateTab(tabs::TabInterface* tab, const GURL& url) {
    content::TestNavigationObserver navigation_observer(tab->GetContents());
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::CURRENT_TAB,
                                  ui::PAGE_TRANSITION_TYPED,
                                  /*is_renderer_initiated=*/false);
    tab->GetContents()->OpenURL(params, base::DoNothing());
    navigation_observer.Wait();
  }

  [[nodiscard]] base::expected<TestTabCreationEvent, std::string>
  OpenURLAndWaitForTabCreation(tabs::TabInterface* source_tab,
                               const content::OpenURLParams& params,
                               GlicTabEventCollector& collector) {
    content::TestNavigationObserver navigation_observer(params.url);
    navigation_observer.StartWatchingNewWebContents();
    source_tab->GetContents()->OpenURL(params, base::DoNothing());
    ASSIGN_OR_RETURN(TestTabCreationEvent creation,
                     collector.WaitForCreation());
    // Only wait for navigation to complete if the tab was actually created.
    // Otherwise, the observer will hang.
    navigation_observer.Wait();
    return creation;
  }

  [[nodiscard]] base::expected<TestTabCreationEvent, std::string>
  ExecJsAndWaitForTabCreation(const content::ToRenderFrameHost& adapter,
                              std::string_view script,
                              const GURL& target_url,
                              GlicTabEventCollector& collector) {
    content::TestNavigationObserver navigation_observer(target_url);
    navigation_observer.StartWatchingNewWebContents();
    if (!content::ExecJs(adapter, script)) {
      return base::unexpected("Failed to execute JS script.");
    }
    ASSIGN_OR_RETURN(TestTabCreationEvent creation,
                     collector.WaitForCreation());
    // Only wait for navigation to complete if the tab was actually created.
    // Otherwise, the observer will hang.
    navigation_observer.Wait();
    return creation;
  }
};

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabCreation) {
  GlicTabEventCollector collector(GetProfile());

  // Initial tab verification
  tabs::TabInterface* initial_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  // Open Tab 2
  tabs::TabInterface* second_tab = CreateTab();
  ASSERT_OK_AND_ASSIGN(auto creation, collector.WaitForCreation());
  EXPECT_NE(creation.new_tab, nullptr);
  EXPECT_EQ(creation.old_tab.get(), initial_tab);
  EXPECT_EQ(creation.new_tab.get(), second_tab);

  // Clear events to ensure we wait for the NEXT creation.
  collector.ClearEvents();

  // Open Tab 3
  tabs::TabInterface* third_tab = CreateTab();
  ASSERT_OK_AND_ASSIGN(creation, collector.WaitForCreation());
  ASSERT_TRUE(creation.new_tab);
  EXPECT_EQ(creation.old_tab.get(), second_tab);
  EXPECT_EQ(creation.new_tab.get(), third_tab);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       ObservesTabCreationInNewWindow) {
  GlicTabEventCollector collector(GetProfile());

  BrowserWindowInterface* new_window = CreateAdditionalBrowserWindow();

  ASSERT_OK_AND_ASSIGN(auto creation, collector.WaitForCreation());
  EXPECT_NE(creation.new_tab, nullptr);
  EXPECT_EQ(creation.new_tab->GetBrowserWindowInterface(), new_window);
}

// Mobile Android does not support programmatically creating separate browser
// window tasks with an OTR profile; attempting to do so triggers a profile
// assertion crash in ChromeAndroidTaskImpl.java at startup.
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       DoesNotObserveTabCreationInDifferentProfile) {
  TabListInterface* incognito_tab_list = CreateIncognitoTabList();
  GlicTabEventCollector collector(GetProfile());

  // Create tab in incognito. Should NOT trigger event.
  tabs::TabInterface* incognito_tab = CreateTab(incognito_tab_list);

  // Create tab in regular profile. Should trigger event.
  tabs::TabInterface* regular_tab = CreateTab();

  ASSERT_OK_AND_ASSIGN(auto creation, collector.WaitForCreation());
  EXPECT_EQ(creation.creation_type, TabCreationType::kUserInitiated);
  EXPECT_EQ(creation.new_tab.get(), regular_tab);

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
  ASSERT_OK(collector.WaitForCreation());

  // Create another tab to keep the browser alive.
  CreateTab();
  ASSERT_OK(collector.WaitForCreation());

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

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabStripMerge) {
  BrowserWindowInterface* window2 = CreateNewWindowWithTab();
  TabListInterface* tab_list2 = TabListInterface::From(window2);
  tabs::TabInterface* tab_to_move = tab_list2->GetActiveTab();

  GlicTabEventCollector collector(GetProfile());

  tab_list2->MoveTabToWindow(tab_to_move->GetHandle(),
                             GetBrowserWindowInterface()->GetSessionID(), 0);

  // We expect both insertion and likely some mutations from the detach/insert.
  ASSERT_OK(collector.WaitForCreation());

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

  window2->GetWindow()->Close();
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       TabMoveDoesNotClassifyAsNewCreation) {
  BrowserWindowInterface* window2 = CreateNewWindowWithTab();
  TabListInterface* tab_list2 = TabListInterface::From(window2);
  tabs::TabInterface* tab_to_move = tab_list2->GetActiveTab();
  NavigateTab(tab_to_move, GURL("about:blank"));

  GlicTabEventCollector collector(GetProfile());

  tab_list2->MoveTabToWindow(tab_to_move->GetHandle(),
                             GetBrowserWindowInterface()->GetSessionID(), 0);

  // 3. Wait for the tab creation event.
  ASSERT_OK_AND_ASSIGN(auto creation, collector.WaitForCreation());

  // 4. Verify that the creation_type is kUnknown because it was a tab move,
  // not a newly created user link or typed tab.
  EXPECT_EQ(creation.creation_type, TabCreationType::kUnknown);

  // 5. Verify there was exactly one creation event in the collector's history.
  int creation_event_count = 0;
  for (const auto& event : collector.events()) {
    if (std::holds_alternative<TestTabCreationEvent>(event)) {
      creation_event_count++;
    }
  }
  EXPECT_EQ(creation_event_count, 1);

  window2->GetWindow()->Close();
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabNavigation) {
  GlicTabEventCollector collector(GetProfile());

  // Create and activate a tab to ensure we have a valid active tab to navigate.
  tabs::TabInterface* tab = CreateTab();
  ASSERT_OK(collector.WaitForCreation());
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
  ASSERT_OK_AND_ASSIGN(auto creation, collector.WaitForCreation());
  EXPECT_EQ(creation.new_tab.get(), second_tab);
  collector.ClearEvents();

  // Switch back to the first tab.
  GetTabListInterface()->ActivateTab(initial_tab->GetHandle());

  ASSERT_OK_AND_ASSIGN(auto activation, collector.WaitForActivation());
  EXPECT_EQ(activation.new_active_tab.get(), initial_tab);
  EXPECT_EQ(activation.old_active_tab.get(), second_tab);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, LinkClickTracking) {
  GlicTabEventCollector collector(GetProfile());

  // 1. Get initial tab
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(first_tab);

  GURL target_url("about:blank");
  content::OpenURLParams params(target_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);
  ASSERT_OK_AND_ASSIGN(auto creation, OpenURLAndWaitForTabCreation(
                                          first_tab, params, collector));
  ASSERT_TRUE(creation.new_tab);

  EXPECT_EQ(creation.creation_type, TabCreationType::kFromLink);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, LinkClickNewWindowTracking) {
  GlicTabEventCollector collector(GetProfile());

  // 1. Get initial tab
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(first_tab);

  GURL target_url("about:blank");
  content::OpenURLParams params(target_url, content::Referrer(),
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

  ASSERT_OK_AND_ASSIGN(auto creation, OpenURLAndWaitForTabCreation(
                                          first_tab, params, collector));
  ASSERT_TRUE(creation.new_tab);

#if !BUILDFLAG(IS_ANDROID)
  // Android forces the NEW_WINDOW disposition to open in the same window,
  // returning the same BrowserWindowInterface pointer. Only assert they are
  // different on other platforms.
  EXPECT_NE(creation.new_tab->GetBrowserWindowInterface(),
            first_tab->GetBrowserWindowInterface());
#endif

  // Verify the opener is preserved
  EXPECT_EQ(creation.opener.get(), first_tab);
  EXPECT_EQ(creation.creation_type, TabCreationType::kFromLink);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, WindowOpenTracking) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GlicTabEventCollector collector(GetProfile());

  // 1. Get initial tab
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(first_tab);

  NavigateTab(first_tab, embedded_test_server()->GetURL("/title1.html"));
  collector.WaitForMutation();
  collector.ClearEvents();

  // 2. Simulate window.open()
  ASSERT_OK_AND_ASSIGN(
      auto creation,
      ExecJsAndWaitForTabCreation(first_tab->GetContents(), "window.open();",
                                  GURL("about:blank"), collector));
  ASSERT_TRUE(creation.new_tab);

  EXPECT_EQ(creation.creation_type, TabCreationType::kFromLink);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       TargetBlankLinkClickTracking) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GlicTabEventCollector collector(GetProfile());

  // 1. Get initial tab
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(first_tab);

  NavigateTab(first_tab, embedded_test_server()->GetURL("/title1.html"));
  collector.WaitForMutation();
  collector.ClearEvents();

  GURL target_url = embedded_test_server()->GetURL("/title2.html");
  std::string script = base::ReplaceStringPlaceholders(
      R"(
        var a = document.createElement('a');
        a.href = '$1';
        a.target = '_blank';
        document.body.appendChild(a);
        a.click();
      )",
      {target_url.spec()}, nullptr);
  ASSERT_OK_AND_ASSIGN(auto creation, ExecJsAndWaitForTabCreation(
                                          first_tab->GetContents(), script,
                                          target_url, collector));
  ASSERT_TRUE(creation.new_tab);

  EXPECT_EQ(creation.creation_type, TabCreationType::kFromLink);
}
