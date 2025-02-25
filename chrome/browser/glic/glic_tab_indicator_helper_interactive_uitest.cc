// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_tab_indicator_helper.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "url/gurl.h"

namespace glic {

namespace {

class TabAlertStateObserver
    : public ui::test::PollingStateObserver<std::vector<TabAlertState>> {
 public:
  TabAlertStateObserver(Browser* browser, int tab_index)
      : PollingStateObserver([browser, tab_index]() {
          auto* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser);
          TabStrip* const tab_strip = browser_view->tabstrip();
          if (tab_index < tab_strip->GetModelCount()) {
            auto* const tab = tab_strip->tab_at(tab_index);
            return tab->data().alert_state;
          }
          return std::vector<TabAlertState>();
        }) {}
  ~TabAlertStateObserver() override = default;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabAlertStateObserver, kTab1AlertState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabAlertStateObserver, kTab2AlertState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabAlertStateObserver, kTab3AlertState);

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);

}  // namespace

class GlicTabIndicatorHelperUiTest : public test::InteractiveGlicTest {
 public:
  GlicTabIndicatorHelperUiTest() = default;
  ~GlicTabIndicatorHelperUiTest() override = default;

  static auto IsAccessing() {
    return testing::Matcher<std::vector<TabAlertState>>(
        testing::Contains(TabAlertState::GLIC_ACCESSING));
  }
  static auto IsNotAccessing() {
    return testing::Matcher<std::vector<TabAlertState>>(
        testing::Not(testing::Contains(TabAlertState::GLIC_ACCESSING)));
  }

  GURL GetTestUrl() const {
    return embedded_test_server()->GetURL("/links.html");
  }

  auto LoadStartingPage() {
    return Steps(InstrumentTab(kFirstTabId),
                 NavigateWebContents(kFirstTabId, GetTestUrl()));
  }

  auto AddNewCandidateTab() {
    return AddInstrumentedTab(kSecondTabId, GetTestUrl());
  }

 protected:
  const InteractiveBrowserTest::DeepQuery kMockGlicContextAccessButton = {
      "#contextAccessIndicator"};
};

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, TabNotAlerted) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  WaitForState(kTab1AlertState, IsNotAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, TabAlerted) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, TabAlertTurnsOff) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsAccessing()),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsNotAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, SecondTabAlerted) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  ObserveState(kTab2AlertState, browser(), 1),
                  AddNewCandidateTab(), SelectTab(kTabStripElementId, 1),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab2AlertState, IsAccessing()),
                  WaitForState(kTab1AlertState, IsNotAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, SwitchAlertedTabs) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser(), 1), AddNewCandidateTab(),
      OpenGlicWindow(GlicWindowMode::kAttached),
      SelectTab(kTabStripElementId, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState, IsAccessing()),
      SelectTab(kTabStripElementId, 1),
      WaitForState(kTab2AlertState, IsAccessing()),
      WaitForState(kTab1AlertState, IsNotAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, AlertChangesOnTabRemoval) {
  static constexpr char kTabCloseButton[] = "tab_close_button";
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser(), 1), AddNewCandidateTab(),
      SelectTab(kTabStripElementId, 1),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab2AlertState, IsAccessing()),
      NameViewRelative(kTabStripElementId, kTabCloseButton,
                       [](TabStrip* tab_strip) {
                         return tab_strip->tab_at(1)->close_button().get();
                       }),
      PressButton(kTabCloseButton),
      WaitForState(kTab1AlertState, IsAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       AlertDoesNotChangeOnTabRemoval) {
  static constexpr char kTabCloseButton[] = "tab_close_button";
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser(), 1), AddNewCandidateTab(),
      SelectTab(kTabStripElementId, 0),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState, IsAccessing()),
      NameViewRelative(kTabStripElementId, kTabCloseButton,
                       [](TabStrip* tab_strip) {
                         return tab_strip->tab_at(1)->close_button().get();
                       }),
      PressButton(kTabCloseButton),
      WaitForState(kTab1AlertState, IsAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, ActiveBrowserAlerted) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  Browser* const browser2 = CreateBrowser(browser()->profile());
  Browser* const browser3 = CreateBrowser(browser()->profile());
  RunTestSequence(LoadStartingPage(), OpenGlicWindow(GlicWindowMode::kDetached),
                  InContext(browser2->window()->GetElementContext(),
                            ActivateSurface(kBrowserViewElementId)),
                  ObserveState(kTab1AlertState, browser(), 0),
                  ObserveState(kTab2AlertState, browser2, 0),
                  ObserveState(kTab3AlertState, browser3, 0),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsNotAccessing()),
                  WaitForState(kTab2AlertState, IsAccessing()),
                  WaitForState(kTab3AlertState, IsNotAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       AlertChangesOnTabMovedBetweenBrowsers) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  Browser* const browser2 = CreateBrowser(browser()->profile());
  RunTestSequence(LoadStartingPage(), OpenGlicWindow(GlicWindowMode::kDetached),
                  ActivateSurface(kBrowserViewElementId),
                  ObserveState(kTab1AlertState, browser(), 1),
                  ObserveState(kTab2AlertState, browser2, 0),
                  ObserveState(kTab3AlertState, browser2, 1),
                  AddNewCandidateTab(), SelectTab(kTabStripElementId, 1),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsAccessing()),
                  // This implicitly activates the second browser.
                  Do([this, browser2]() {
                    chrome::MoveTabsToExistingWindow(browser(), browser2, {1});
                  }),
                  WaitForState(kTab3AlertState, IsAccessing()),
                  WaitForState(kTab1AlertState, IsNotAccessing()),
                  InContext(browser2->window()->GetElementContext(),
                            SelectTab(kTabStripElementId, 0)),
                  WaitForState(kTab2AlertState, IsAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       AcessingTabAfterOpeningTabSearchDialog) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsAccessing()),
                  PressButton(kTabSearchButtonElementId),
                  WaitForShow(kTabSearchBubbleElementId),
                  WaitForState(kTab1AlertState, IsAccessing()));
}

}  // namespace glic
