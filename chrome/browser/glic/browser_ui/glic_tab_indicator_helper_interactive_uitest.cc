// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_tab_indicator_helper.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/ui_test_utils.h"
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
    : public ui::test::PollingStateObserver<std::vector<tabs::TabAlert>> {
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
          return std::vector<tabs::TabAlert>();
        }) {}
  ~TabAlertStateObserver() override = default;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabAlertStateObserver, kTab1AlertState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabAlertStateObserver, kTab2AlertState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabAlertStateObserver, kTab3AlertState);

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTabId);

}  // namespace

class GlicTabIndicatorHelperUiTest : public test::InteractiveGlicTest {
 public:
  GlicTabIndicatorHelperUiTest() = default;
  ~GlicTabIndicatorHelperUiTest() override = default;

  static auto IsAccessing() {
    return testing::Matcher<std::vector<tabs::TabAlert>>(
        testing::Contains(tabs::TabAlert::GLIC_ACCESSING));
  }
  static auto IsNotAccessing() {
    return testing::Matcher<std::vector<tabs::TabAlert>>(
        testing::Not(testing::Contains(tabs::TabAlert::GLIC_ACCESSING)));
  }

  GURL GetTestUrl() const {
    return embedded_test_server()->GetURL("/links.html");
  }

  auto LoadStartingPage() {
    return Steps(InstrumentTab(kFirstTabId),
                 NavigateWebContents(kFirstTabId, GetTestUrl()));
  }

  auto LoadStartingPage(ui::ElementIdentifier id,
                        std::optional<int> tab_index,
                        BrowserSpecifier in_browser) {
    return Steps(InstrumentTab(id, tab_index, in_browser),
                 NavigateWebContents(id, GetTestUrl()));
  }

  auto AddNewCandidateTab(ui::ElementIdentifier id) {
    return AddInstrumentedTab(id, GetTestUrl());
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
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser(), 1),
      AddNewCandidateTab(kSecondTabId), SelectTab(kTabStripElementId, 1),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab2AlertState, IsAccessing()),
      WaitForState(kTab1AlertState, IsNotAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, SwitchAlertedTabs) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  ObserveState(kTab2AlertState, browser(), 1),
                  AddNewCandidateTab(kSecondTabId),
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
      ObserveState(kTab2AlertState, browser(), 1),
      AddNewCandidateTab(kSecondTabId), SelectTab(kTabStripElementId, 1),
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
      ObserveState(kTab2AlertState, browser(), 1),
      AddNewCandidateTab(kSecondTabId), SelectTab(kTabStripElementId, 0),
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

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       NavigatingToInvalidSchemeShouldNotAlert) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsAccessing()),
                  NavigateWebContents(kFirstTabId, GURL("chrome://settings")),
                  WaitForState(kTab1AlertState, IsNotAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       NavigatingToAllowlistedUrlShouldAlert) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsAccessing()),
                  NavigateWebContents(kFirstTabId, GURL("chrome://newtab/")),
                  WaitForState(kTab1AlertState, IsAccessing()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       NavigatingToInvalidSchemeAndBackShouldAlert) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsAccessing()),
                  NavigateWebContents(kFirstTabId, GURL("chrome://settings")),
                  WaitForState(kTab1AlertState, IsNotAccessing()),
                  NavigateWebContents(kFirstTabId, GetTestUrl()),
                  WaitForState(kTab1AlertState, IsAccessing()));
}

// TODO(crbug.com/396768066): Fix and re-enable this test.
IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       DISABLED_ActiveBrowserAlerted) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  Browser* const browser2 = CreateBrowser(browser()->profile());
  Browser* const browser3 = CreateBrowser(browser()->profile());
  RunTestSequence(LoadStartingPage(),
                  LoadStartingPage(kSecondTabId, 0, browser2),
                  LoadStartingPage(kThirdTabId, 0, browser3),
                  OpenGlicWindow(GlicWindowMode::kDetached),
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
                       IncognitoBrowserShouldNotAlert) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  Browser* const browser2 = CreateIncognitoBrowser(browser()->profile());
  RunTestSequence(LoadStartingPage(), OpenGlicWindow(GlicWindowMode::kDetached),
                  ObserveState(kTab1AlertState, browser(), 0),
                  ObserveState(kTab2AlertState, browser2, 0),
                  ClickMockGlicElement(kMockGlicContextAccessButton),
                  WaitForState(kTab1AlertState, IsAccessing()),
                  InContext(browser2->window()->GetElementContext(),
                            ActivateSurface(kBrowserViewElementId)),
                  WaitForState(kTab1AlertState, IsAccessing()),
                  WaitForState(kTab2AlertState, IsNotAccessing()));
}

// TODO(crbug.com/404281597): Re-enable this test on Linux.
// TODO(crbug.com/408424752): Re-enable this flakily-failing test on Mac.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_MinimizingWindowWithGlicDetachedShouldNotAlertUntilNewBrowserActive \
  DISABLED_MinimizingWindowWithGlicDetachedShouldNotAlertUntilNewBrowserActive
#else
#define MAYBE_MinimizingWindowWithGlicDetachedShouldNotAlertUntilNewBrowserActive \
  MinimizingWindowWithGlicDetachedShouldNotAlertUntilNewBrowserActive
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(
    GlicTabIndicatorHelperUiTest,
    MAYBE_MinimizingWindowWithGlicDetachedShouldNotAlertUntilNewBrowserActive) {
  Browser* const browser2 = CreateBrowser(browser()->profile());
  RunTestSequence(
      LoadStartingPage(), LoadStartingPage(kSecondTabId, 0, browser2),
      OpenGlicWindow(GlicWindowMode::kDetached),
      ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser2, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab2AlertState, IsAccessing()), Do([browser2]() {
        browser2->window()->Minimize();
        ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser2));
      }),
      WaitForState(kTab2AlertState, IsNotAccessing()),
      InContext(browser()->window()->GetElementContext(),
                ActivateSurface(kBrowserViewElementId)),
      WaitForState(kTab2AlertState, IsNotAccessing()),
      WaitForState(kTab1AlertState, IsAccessing()));
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
  RunTestSequence(
      LoadStartingPage(), LoadStartingPage(kSecondTabId, 0, browser2),
      OpenGlicWindow(GlicWindowMode::kDetached),
      ActivateSurface(kBrowserViewElementId),
      ObserveState(kTab1AlertState, browser(), 1),
      ObserveState(kTab2AlertState, browser2, 0),
      ObserveState(kTab3AlertState, browser2, 1),
      AddNewCandidateTab(kThirdTabId), SelectTab(kTabStripElementId, 1),
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
