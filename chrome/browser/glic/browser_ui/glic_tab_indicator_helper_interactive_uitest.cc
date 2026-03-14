// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_tab_indicator_helper.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab/tab_close_button.h"
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

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);

}  // namespace

class GlicTabIndicatorHelperMultiInstanceUiTest
    : public test::InteractiveGlicTest,
      public testing::WithParamInterface<bool> {
 public:
  GlicTabIndicatorHelperMultiInstanceUiTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicMultiInstance,
                              glic::mojom::features::kGlicMultiTab,
                              features::kGlicMultitabUnderlines},
        /*disabled_features=*/{});
  }
  ~GlicTabIndicatorHelperMultiInstanceUiTest() override = default;

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

  auto OpenGlicInInteractionMode(GlicWindowMode mode) {
    return Steps(DeprecatedOpenGlicWindow(mode), Do([this]() {
                   if (auto* instance = GetGlicInstanceImpl()) {
                     instance->OnInteractionModeChange(
                         GetParam() ? mojom::WebClientMode::kAudio
                                    : mojom::WebClientMode::kText);
                   }
                 }));
  }

  std::optional<tabs::TabAlert> ExpectedAlertState() {
    return GetParam() ? std::make_optional(tabs::TabAlert::kGlicAccessing)
                      : std::make_optional(tabs::TabAlert::kGlicSharing);
  }

  auto ClickMockGlicContextAccessButtonIfLiveMode() {
    return Steps(If([this]() { return this->GetParam(); },
                    Then(ClickMockGlicElement(kMockGlicContextAccessButton))));
  }

 protected:
  const InteractiveBrowserTest::DeepQuery kMockGlicContextAccessButton = {
      "#contextAccessIndicator"};

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       TabNotAlerted) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicSharing)));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest, TabAlerted) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicInInteractionMode(GlicWindowMode::kAttached),
                  ClickMockGlicContextAccessButtonIfLiveMode(),
                  WaitForState(kTab1AlertState, ExpectedAlertState()));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       TabAlertTurnsOff) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicInInteractionMode(GlicWindowMode::kAttached),
      ClickMockGlicContextAccessButtonIfLiveMode(),
      WaitForState(kTab1AlertState, ExpectedAlertState()),
      ClickMockGlicContextAccessButtonIfLiveMode(),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicSharing)));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       SecondTabAlerted) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  TrackOnlyGlicInstance();
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  AddNewCandidateTab(kSecondTabId),
                  ObserveState(kTab2AlertState, browser(), 1),
                  SelectTab(kTabStripElementId, 1),
                  OpenGlicInInteractionMode(GlicWindowMode::kAttached),
                  ClickMockGlicContextAccessButtonIfLiveMode(),
                  WaitForState(kTab2AlertState, ExpectedAlertState()),
                  WaitForState(kTab1AlertState, std::nullopt));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       NavigatingToInvalidSchemeShouldNotAlert) {
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicInInteractionMode(GlicWindowMode::kAttached),
      ClickMockGlicContextAccessButtonIfLiveMode(),
      WaitForState(kTab1AlertState, ExpectedAlertState()),
      NavigateWebContents(kFirstTabId, GURL("chrome://settings")),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicSharing)));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       NavigatingToAllowlistedUrlShouldAlert) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicInInteractionMode(GlicWindowMode::kAttached),
                  ClickMockGlicContextAccessButtonIfLiveMode(),
                  WaitForState(kTab1AlertState, ExpectedAlertState()),
                  NavigateWebContents(kFirstTabId, GURL("chrome://newtab/")),
                  WaitForState(kTab1AlertState, ExpectedAlertState()));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       AcessingTabAfterOpeningTabSearchDialog) {
  RunTestSequence(LoadStartingPage(),
                  ObserveState(kTab1AlertState, browser(), 0),
                  OpenGlicInInteractionMode(GlicWindowMode::kAttached),
                  ClickMockGlicContextAccessButtonIfLiveMode(),
                  WaitForState(kTab1AlertState, ExpectedAlertState()),
                  PressButton(kTabSearchButtonElementId),
                  WaitForShow(kTabSearchBubbleElementId), Check([this]() {
                    return GetTabAlertControllerForTab(browser(), 0)
                               ->GetAlertToShow()
                               .value() == ExpectedAlertState();
                  }));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       SwitchAlertedTabs) {
  RunTestSequence(LoadStartingPage(),
                  Do([this]() { TrackGlicInstanceWithTabIndex(0); }),
                  ObserveState(kTab1AlertState, browser(), 0),
                  AddNewCandidateTab(kSecondTabId),
                  ObserveState(kTab2AlertState, browser(), 1),
                  SelectTab(kTabStripElementId, 0),
                  OpenGlicInInteractionMode(GlicWindowMode::kAttached),
                  ClickMockGlicContextAccessButtonIfLiveMode(),
                  WaitForState(kTab1AlertState, ExpectedAlertState()),
                  SelectTab(kTabStripElementId, 1),
                  WaitForState(kTab1AlertState, std::nullopt),
                  Do([this]() { TrackGlicInstanceWithTabIndex(1); }),
                  OpenGlicInInteractionMode(GlicWindowMode::kAttached),
                  ClickMockGlicContextAccessButtonIfLiveMode(),
                  WaitForState(kTab2AlertState, ExpectedAlertState()));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       AlertChangesOnTabRemoval) {
  static constexpr char kTabCloseButton[] = "tab_close_button";
  TrackGlicInstanceWithTabIndex(0);
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      AddNewCandidateTab(kSecondTabId),
      ObserveState(kTab2AlertState, browser(), 1),
      SelectTab(kTabStripElementId, 1),
      Do([this]() { TrackGlicInstanceWithTabIndex(1); }),
      OpenGlicInInteractionMode(GlicWindowMode::kAttached),
      ClickMockGlicContextAccessButtonIfLiveMode(),
      WaitForState(kTab2AlertState, ExpectedAlertState()),
      NameViewRelative(kTabStripElementId, kTabCloseButton,
                       [](TabStrip* tab_strip) {
                         return tab_strip->tab_at(1)->close_button().get();
                       }),
      PressButton(kTabCloseButton),
      // Tab 1 had no instance opened on it in this test sequence.
      WaitForState(kTab1AlertState, std::nullopt));
}

IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       NavigatingToInvalidSchemeAndBackShouldAlert) {
  TrackGlicInstanceWithTabIndex(0);
  RunTestSequence(
      LoadStartingPage(), ObserveState(kTab1AlertState, browser(), 0),
      OpenGlicInInteractionMode(GlicWindowMode::kAttached),
      ClickMockGlicContextAccessButtonIfLiveMode(),
      WaitForState(kTab1AlertState, ExpectedAlertState()),
      NavigateWebContents(kFirstTabId, GURL("chrome://settings")),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicSharing)),
      NavigateWebContents(kFirstTabId, GetTestUrl()),
      WaitForState(kTab1AlertState, ExpectedAlertState()));
}

// TODO(b/489122337): This test worked with single-instance mode, but does not
// work with MI.
IN_PROC_BROWSER_TEST_P(GlicTabIndicatorHelperMultiInstanceUiTest,
                       DISABLED_AlertChangesOnTabMovedBetweenBrowsers) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  Browser* const browser1 = browser();
  TrackOnlyGlicInstance();
  RunTestSequence(
      LoadStartingPage(), AddInstrumentedTab(kSecondTabId, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), OpenGlicFloatingWindow(),
      ClickMockGlicContextAccessButtonIfLiveMode(),
      WaitForState(kTab1AlertState, ExpectedAlertState()),
      Do([this, browser1]() {
        Browser* const browser2 = CreateBrowser(browser1->profile());
        RunTestSequence(
            ObserveState(kTab1AlertState, browser1, 0),
            ObserveState(kTab2AlertState, browser1, 1),
            // This implicitly activates the second browser.
            Do([browser1, browser2]() {
              chrome::MoveTabsToExistingWindow(browser1, browser2, {0});
            }),
            // Index 1 (Tab 2) is now Index 0 in browser1.
            // Tab 1 is now Index 1 in browser2.
            InstrumentTab(kFirstTabId, 1, browser2),
            // In multi-instance mode, switching away from a tab (even by move)
            // might hide its indicator if it's not the active instance of the
            // new browser.
            WaitForState(kTab1AlertState,
                         std::make_optional(tabs::TabAlert::kGlicSharing)),
            InContext(BrowserElements::From(browser2)->GetContext(),
                      SelectTab(kTabStripElementId, 1)),
            // When selected in the new browser, it should stay sharing or move
            // to accessing if Glic is re-opened.
            WaitForState(kTab1AlertState,
                         std::make_optional(tabs::TabAlert::kGlicSharing)));
      }));
}

INSTANTIATE_TEST_SUITE_P(,
                         GlicTabIndicatorHelperMultiInstanceUiTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "LiveMode" : "TextMode";
                         });

}  // namespace glic
