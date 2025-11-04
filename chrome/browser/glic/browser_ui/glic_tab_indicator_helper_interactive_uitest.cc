// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_tab_indicator_helper.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace glic {

namespace {

#if BUILDFLAG(IS_MAC)
bool kTestDisabledForVirtualMachineMac =
    (base::mac::MacOSMajorVersion() == 15) && base::mac::IsVirtualMachine();
#endif  // BUILDFLAG(IS_MAC)

tabs::TabAlertController* GetTabAlertControllerForTab(Browser* browser,
                                                      int tab_index) {
  return tabs::TabAlertController::From(
      browser->tab_strip_model()->GetTabAtIndex(tab_index));
}

class TabAlertStateObserver
    : public ui::test::StateObserver<std::optional<tabs::TabAlert>> {
 public:
  TabAlertStateObserver(Browser* browser, int tab_index) {
    alert_to_show_changed_subscription_ =
        GetTabAlertControllerForTab(browser, tab_index)
            ->AddAlertToShowChangedCallback(base::BindRepeating(
                &TabAlertStateObserver::OnAlertToShowChanged,
                base::Unretained(this)));
  }

  void OnAlertToShowChanged(std::optional<tabs::TabAlert> alert) {
    OnStateObserverStateChanged(alert);
  }

 private:
  base::CallbackListSubscription alert_to_show_changed_subscription_;
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
  GlicTabIndicatorHelperUiTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
  }
  ~GlicTabIndicatorHelperUiTest() override = default;

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

  std::optional<tabs::TabAlert> DefaultExpectedTabAlertWithGlic() {
    // In multi-instance, the tab will be shared initially.
    return base::FeatureList::IsEnabled(features::kGlicMultiInstance)
               ? std::make_optional(tabs::TabAlert::kGlicSharing)
               : std::nullopt;
  }

 protected:
  const InteractiveBrowserTest::DeepQuery kMockGlicContextAccessButton = {
      "#contextAccessIndicator"};
};

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, TabNotAlerted) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicWindow(GlicWindowMode::kAttached),

      WaitForState(kTab1AlertState,
                   // In multi-instance, the tab will be shared initially.
                   DefaultExpectedTabAlertWithGlic()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, TabAlerted) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, TabAlertTurnsOff) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState, DefaultExpectedTabAlertWithGlic()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, SecondTabAlerted) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  TrackOnlyGlicInstance();
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      AddNewCandidateTab(kSecondTabId),
      ObserveState(kTab2AlertState, browser(), 1),
      SelectTab(kTabStripElementId, 1),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab2AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      WaitForState(kTab1AlertState, std::nullopt));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, SwitchAlertedTabs) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  TrackOnlyGlicInstance();
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      AddNewCandidateTab(kSecondTabId),
      ObserveState(kTab2AlertState, browser(), 1),
      OpenGlicWindow(GlicWindowMode::kAttached),
      SelectTab(kTabStripElementId, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      SelectTab(kTabStripElementId, 1),
      WaitForState(kTab2AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      WaitForState(kTab1AlertState, std::nullopt));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest, AlertChangesOnTabRemoval) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  static constexpr char kTabCloseButton[] = "tab_close_button";
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      AddNewCandidateTab(kSecondTabId),
      ObserveState(kTab2AlertState, browser(), 1),
      SelectTab(kTabStripElementId, 1),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab2AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      NameViewRelative(kTabStripElementId, kTabCloseButton,
                       [](TabStrip* tab_strip) {
                         return tab_strip->tab_at(1)->close_button().get();
                       }),
      PressButton(kTabCloseButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       AlertDoesNotChangeOnTabRemoval) {
  static constexpr char kTabCloseButton[] = "tab_close_button";
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      AddNewCandidateTab(kSecondTabId),
      ObserveState(kTab2AlertState, browser(), 1),
      SelectTab(kTabStripElementId, 0),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      NameViewRelative(kTabStripElementId, kTabCloseButton,
                       [](TabStrip* tab_strip) {
                         return tab_strip->tab_at(1)->close_button().get();
                       }),
      PressButton(kTabCloseButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       NavigatingToInvalidSchemeShouldNotAlert) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      NavigateWebContents(kFirstTabId, GURL("chrome://settings")),
      WaitForState(kTab1AlertState, DefaultExpectedTabAlertWithGlic()));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       NavigatingToAllowlistedUrlShouldAlert) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      NavigateWebContents(kFirstTabId, GURL("chrome://newtab/")),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       NavigatingToInvalidSchemeAndBackShouldAlert) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      NavigateWebContents(kFirstTabId, GURL("chrome://settings")),
      WaitForState(kTab1AlertState, DefaultExpectedTabAlertWithGlic()),
      NavigateWebContents(kFirstTabId, GetTestUrl()),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
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
  RunTestSequence(
      LoadStartingPage(), LoadStartingPage(kSecondTabId, 0, browser2),
      LoadStartingPage(kThirdTabId, 0, browser3),
      OpenGlicWindow(GlicWindowMode::kDetached),
      InContext(BrowserElements::From(browser2)->GetContext(),
                ActivateSurface(kBrowserViewElementId)),
      ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser2, 0),
      ObserveState(kTab3AlertState, browser3, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState, std::nullopt),
      WaitForState(kTab2AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      WaitForState(kTab3AlertState, std::nullopt));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       IncognitoBrowserShouldNotAlert) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  Browser* const browser2 = CreateIncognitoBrowser(browser()->profile());
  RunTestSequence(
      LoadStartingPage(), OpenGlicWindow(GlicWindowMode::kDetached),
      ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser2, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      ActivateSurface(kBrowserViewElementId),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      InContext(BrowserElements::From(browser2)->GetContext(),
                ActivateSurface(kBrowserViewElementId)),
      WaitForState(kTab1AlertState, std::nullopt),
      WaitForState(kTab2AlertState, std::nullopt));
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
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    GTEST_SKIP() << "Test does not work for multi-instance.";
  }
  Browser* const browser2 = CreateBrowser(browser()->profile());
  RunTestSequence(
      LoadStartingPage(), LoadStartingPage(kSecondTabId, 0, browser2),
      OpenGlicWindow(GlicWindowMode::kDetached),
      ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser2, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab2AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      Do([browser2]() {
        browser2->window()->Minimize();
        ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser2));
      }),
      WaitForState(kTab2AlertState, DefaultExpectedTabAlertWithGlic()),
      ActivateSurface(kBrowserViewElementId),
      WaitForState(kTab2AlertState, DefaultExpectedTabAlertWithGlic()),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       AlertChangesOnTabMovedBetweenBrowsers) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
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
      ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser2, 0),
      AddNewCandidateTab(kThirdTabId),
      ObserveState(kTab3AlertState, browser(), 1),
      SelectTab(kTabStripElementId, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      // This implicitly activates the second browser.
      Do([this, browser2]() {
        chrome::MoveTabsToExistingWindow(browser(), browser2, {1});
      }),
      WaitForState(kTab3AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      WaitForState(kTab1AlertState, std::nullopt),
      InContext(BrowserElements::From(browser2)->GetContext(),
                SelectTab(kTabStripElementId, 0)),
      WaitForState(kTab2AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
}

IN_PROC_BROWSER_TEST_F(GlicTabIndicatorHelperUiTest,
                       AcessingTabAfterOpeningTabSearchDialog) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      PressButton(kTabSearchButtonElementId),
      WaitForShow(kTabSearchBubbleElementId), Check([this]() {
        return GetTabAlertControllerForTab(browser(), 0)
                   ->GetAlertToShow()
                   .value() == tabs::TabAlert::kGlicAccessing;
      }));
}

}  // namespace glic
