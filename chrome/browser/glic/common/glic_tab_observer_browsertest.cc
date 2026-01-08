// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_observer.h"

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

class GlicTabObserverBrowserTest : public InProcessBrowserTest {
 public:
  GlicTabObserverBrowserTest() = default;
  ~GlicTabObserverBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest, ObservesTabCreation) {
  base::test::TestFuture<const GlicTabEvent&> future;
  auto observer = GlicTabObserver::Create(browser()->profile(),
                                          future.GetRepeatingCallback());

  // There is already an initial tab.
  tabs::TabInterface* initial_tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(initial_tab);

  // Open a new tab (Tab 2).
  chrome::NewTab(browser());
  {
    const GlicTabEvent& event = future.Take();
    const auto* creation_event = std::get_if<TabCreationEvent>(&event);
    ASSERT_TRUE(creation_event);
    EXPECT_NE(creation_event->new_tab, nullptr);
    EXPECT_EQ(creation_event->old_tab, initial_tab);
    EXPECT_EQ(creation_event->creation_type, TabCreationType::kUserInitiated);

    // Verify it matches the actual new tab.
    EXPECT_EQ(creation_event->new_tab, browser()->GetActiveTabInterface());
  }

  // Open another tab (Tab 3).
  // This verifies that we update our tracking of the "old" tab correctly.
  tabs::TabInterface* second_tab = browser()->GetActiveTabInterface();
  chrome::NewTab(browser());
  {
    const GlicTabEvent& event = future.Take();
    const auto* creation_event = std::get_if<TabCreationEvent>(&event);
    ASSERT_TRUE(creation_event);
    EXPECT_NE(creation_event->new_tab, nullptr);
    EXPECT_EQ(creation_event->old_tab, second_tab);
    EXPECT_EQ(creation_event->creation_type, TabCreationType::kUserInitiated);
    EXPECT_EQ(creation_event->new_tab, browser()->GetActiveTabInterface());
  }
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       ObservesTabCreationInNewWindow) {
  base::test::TestFuture<const GlicTabEvent&> future;
  auto observer = GlicTabObserver::Create(browser()->profile(),
                                          future.GetRepeatingCallback());

  chrome::NewEmptyWindow(browser()->profile());

  // Note: NewEmptyWindow creates a browser with an initial tab.
  const GlicTabEvent& event = future.Take();
  const auto* creation_event = std::get_if<TabCreationEvent>(&event);
  ASSERT_TRUE(creation_event);
  EXPECT_NE(creation_event->new_tab, nullptr);
}

IN_PROC_BROWSER_TEST_F(GlicTabObserverBrowserTest,
                       DoesNotObserveTabCreationInDifferentProfile) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  base::test::TestFuture<const GlicTabEvent&> future;
  auto observer = GlicTabObserver::Create(browser()->profile(),
                                          future.GetRepeatingCallback());

  // Create tab in incognito. Should NOT trigger event.
  chrome::NewTab(incognito_browser);

  EXPECT_FALSE(future.IsReady());

  // Create tab in regular profile. Should trigger event.
  chrome::NewTab(browser());

  const GlicTabEvent& event = future.Take();
  const auto* creation_event = std::get_if<TabCreationEvent>(&event);
  ASSERT_TRUE(creation_event);
  EXPECT_EQ(creation_event->creation_type, TabCreationType::kUserInitiated);
  // Verify it's the regular tab, not the incognito one.
  EXPECT_EQ(creation_event->new_tab, browser()->GetActiveTabInterface());
  EXPECT_NE(creation_event->new_tab,
            incognito_browser->GetActiveTabInterface());
}
