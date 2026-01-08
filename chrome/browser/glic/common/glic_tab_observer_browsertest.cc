// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_observer.h"

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"

class GlicTabEventCollector {
 public:
  explicit GlicTabEventCollector(Profile* profile) {
    observer_ = GlicTabObserver::Create(
        profile, base::BindRepeating(&GlicTabEventCollector::OnEvent,
                                     base::Unretained(this)));
  }

  void OnEvent(const GlicTabEvent& event) {
    events_.push_back(event);
    if (predicate_ && predicate_.Run(event)) {
      condition_met_signal_.SetValue();
    }
  }

  void WaitForEvent(
      base::RepeatingCallback<bool(const GlicTabEvent&)> predicate) {
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

  const TabCreationEvent* WaitForCreation() {
    WaitForEvent(base::BindRepeating([](const GlicTabEvent& event) {
      return std::holds_alternative<TabCreationEvent>(event);
    }));
    for (auto& event : base::Reversed(events_)) {
      if (const auto* c = std::get_if<TabCreationEvent>(&event)) {
        return c;
      }
    }
    return nullptr;
  }

  void WaitForMutation() {
    WaitForEvent(base::BindRepeating([](const GlicTabEvent& event) {
      return std::holds_alternative<TabMutationEvent>(event);
    }));
  }

  const std::vector<GlicTabEvent>& events() const { return events_; }

  void ClearEvents() { events_.clear(); }

 private:
  std::unique_ptr<GlicTabObserver> observer_;
  std::vector<GlicTabEvent> events_;
  base::RepeatingCallback<bool(const GlicTabEvent&)> predicate_;
  base::test::TestFuture<void> condition_met_signal_;
};

class GlicTabObserverBrowserTest : public InProcessBrowserTest {
 public:
  GlicTabObserverBrowserTest() = default;
  ~GlicTabObserverBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabCreation) {
  GlicTabEventCollector collector(browser()->profile());

  // Initial tab verification
  tabs::TabInterface* initial_tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(initial_tab);

  // Open Tab 2
  chrome::NewTab(browser());
  const TabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_NE(creation->new_tab, nullptr);
  EXPECT_EQ(creation->old_tab, initial_tab);
  EXPECT_EQ(creation->new_tab, browser()->GetActiveTabInterface());

  // Clear events to ensure we wait for the NEXT creation.
  collector.ClearEvents();

  // Open Tab 3
  tabs::TabInterface* second_tab = browser()->GetActiveTabInterface();
  chrome::NewTab(browser());
  creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_NE(creation->new_tab, nullptr);
  EXPECT_EQ(creation->old_tab, second_tab);
  EXPECT_EQ(creation->new_tab, browser()->GetActiveTabInterface());
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       ObservesTabCreationInNewWindow) {
  GlicTabEventCollector collector(browser()->profile());

  chrome::NewEmptyWindow(browser()->profile());

  const TabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_NE(creation->new_tab, nullptr);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       DoesNotObserveTabCreationInDifferentProfile) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  GlicTabEventCollector collector(browser()->profile());

  // Create tab in incognito. Should NOT trigger event.
  chrome::NewTab(incognito_browser);

  // Create tab in regular profile. Should trigger event.
  chrome::NewTab(browser());

  const TabCreationEvent* creation = collector.WaitForCreation();
  ASSERT_TRUE(creation);
  EXPECT_EQ(creation->creation_type, TabCreationType::kUserInitiated);
  EXPECT_EQ(creation->new_tab, browser()->GetActiveTabInterface());

  // Verify none of the events were for the incognito browser.
  for (const auto& event : collector.events()) {
    if (const auto* c = std::get_if<TabCreationEvent>(&event)) {
      EXPECT_NE(c->new_tab, incognito_browser->GetActiveTabInterface());
    }
  }
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabMutation) {
  GlicTabEventCollector collector(browser()->profile());

  // Create a tab so we can close it.
  chrome::NewTab(browser());
  collector.WaitForCreation();
  collector.ClearEvents();

  // Close the tab. This should trigger a TabMutationEvent.
  chrome::CloseTab(browser());
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
  GlicTabEventCollector collector(browser()->profile());

  // Create two tabs so we can move one.
  chrome::NewTab(browser());
  collector.WaitForCreation();
  collector.ClearEvents();

  chrome::NewTab(browser());
  collector.WaitForCreation();
  collector.ClearEvents();

  // Move the active tab to the first position.
  browser()->tab_strip_model()->MoveWebContentsAt(2, 0, false);
  collector.WaitForMutation();
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabStripMerge) {
  Browser* browser2 = CreateBrowser(browser()->profile());
  GlicTabEventCollector collector(browser()->profile());

  std::unique_ptr<content::WebContents> contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  browser()->tab_strip_model()->InsertWebContentsAt(0, std::move(contents),
                                                    AddTabTypes::ADD_NONE);

  // We expect both insertion and likely some mutations from the detach/insert.
  collector.WaitForCreation();

  bool found_removal = false;
  bool found_insertion = false;

  for (const auto& event : collector.events()) {
    if (std::holds_alternative<TabMutationEvent>(event)) {
      found_removal = true;
    } else if (const auto* creation = std::get_if<TabCreationEvent>(&event)) {
      if (creation->new_tab) {
        found_insertion = true;
      }
    }
  }

  EXPECT_TRUE(found_removal);
  EXPECT_TRUE(found_insertion);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabNavigation) {
  GlicTabEventCollector collector(browser()->profile());

  // Navigate. This should trigger updates (e.g. loading state change).
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  collector.WaitForMutation();
}
