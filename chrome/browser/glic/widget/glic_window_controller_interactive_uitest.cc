// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/memory_pressure_monitor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/test/button_test_api.h"

namespace glic {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

const InteractiveBrowserTestApi::DeepQuery kMockGlicClientHangButton = {
    "#hang"};

}  // anonymous namespace

class GlicWindowControllerUiTest : public test::InteractiveGlicTest {
 public:
  GlicWindowControllerUiTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
  }
  ~GlicWindowControllerUiTest() override = default;

  auto SimulateGlicHotkey() {
    // TODO: Actually implement the hotkey when we know what it is.
    return Do([this]() {
      glic_service()->ToggleUI(nullptr, /*prevent_close=*/false,
                               mojom::InvocationSource::kOsHotkey);
    });
  }

  auto SimulateOpenMenuItem() {
    return Do([this]() {
      glic_controller_->Show(mojom::InvocationSource::kOsButtonMenu);
    });
  }

  auto SimulateOsButton() {
    return Do([this]() {
      glic_controller_->Toggle(mojom::InvocationSource::kOsButton);
    });
  }

  auto ForceInvalidateAccount() {
    return Do([this]() { InvalidateAccount(window_controller().profile()); });
  }

  auto ForceReauthAccount() {
    return Do([this]() { ReauthAccount(window_controller().profile()); });
  }

 private:
  std::unique_ptr<GlicController> glic_controller_ =
      std::make_unique<GlicController>();
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, ShowAndCloseDetachedWidget) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached));
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DoNotCrashWhenReopening) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached), CloseGlicWindow(),
                  OpenGlicWindow(GlicWindowMode::kAttached));
}

// Disabled due to flakes Mac; see https://crbug.com/394350688.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_OpenDetachedAndThenOpenAttached) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  PressButton(kGlicButtonElementId),
                  WaitForEvent(kGlicButtonElementId, kGlicWidgetAttached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

constexpr char kActivateSurfaceIncompatibilityNotice[] =
    "Programmatic window activation does not work on the Weston reference "
    "implementation of Wayland used on Linux testbots. It also doesn't work "
    "reliably on Linux in general. For this reason, some of these tests which "
    "use ActivateSurface() may be skipped on machine configurations which do "
    "not reliably support them.";

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ButtonWhenAttachedToActiveBrowserCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId),
      // Glic should close.
      PressButton(kGlicButtonElementId),
      InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyWhenDetachedActiveCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateGlicHotkey(), InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

// TODO(393203136): Once tests can observe window controller state rather than
// polling, make a test like this one with glic initially attached.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyDetachedWithNotNormalBrowser) {
  RunTestSequence(
      Do([&]() {
        Browser* const pwa =
            CreateBrowserForApp("app name", browser()->profile());
        pwa->window()->Activate();
      }),
      SimulateGlicHotkey(),
      InAnyContext(WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)),
      CheckControllerWidgetMode(GlicWindowMode::kDetached));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyOpensDetachedWithMinimizedBrowser) {
  RunTestSequence(
      // Glic should open attached to active browser.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId));
  browser()->window()->Minimize();
  ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser()));
  RunTestSequence(
      SimulateGlicHotkey(),
      InAnyContext(WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kDetached));
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyOpensDetachedWithNonActiveBrowser) {
  RunTestSequence(
      // Glic should open attached to active browser.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId));

  // This will make some other window the foreground window.
  browser()->window()->Deactivate();

  RunTestSequence(
      SimulateGlicHotkey(),
      InAnyContext(WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kDetached));
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ESCWhenDetachedActiveCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateAcceleratorPress(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ESCWhenAttachedActiveCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateAcceleratorPress(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

// TODO: Re-nable this test when there is a glic state for post-resize.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_CloseWithContextMenu) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached), CheckControllerHasWidget(true),
      MoveMouseTo(kGlicViewElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT), WaitForHide(kBrowserViewElementId),
          InAnyContext(
              SelectMenuItem(RenderViewContextMenu::kGlicCloseMenuItem))),
      CheckControllerHasWidget(false));
}

// Flaky on macOS: https://crbug.com/401158115
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenMenuItemShows DISABLED_OpenMenuItemShows
#else
#define MAYBE_OpenMenuItemShows OpenMenuItemShows
#endif
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, MAYBE_OpenMenuItemShows) {
  RunTestSequence(SimulateOpenMenuItem(),
                  WaitForAndInstrumentGlic(kHostAndContents),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

#if BUILDFLAG(IS_WIN)
// On Windows, the OsButton toggles opening and closing floaty, because floaty
// will never be active when the os button is clicked.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, OsButtonToggles) {
  RunTestSequence(
      SimulateOsButton(), WaitForAndInstrumentGlic(kHostAndContents),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kDetached), SimulateOsButton(),
      WaitForHide(test::kGlicHostElementId), CheckControllerHasWidget(false));
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenMenuItemWhenAttachedToActiveBrowserDoesNotClose) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      // Glic should close.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId), SimulateOpenMenuItem(),
      CheckControllerShowing(true));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenMenuItemWhenDetachedActiveDoesNotClose) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateOpenMenuItem(), CheckControllerShowing(true));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpeningProfilePickerClosesPanel) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      CheckControllerWidgetMode(GlicWindowMode::kDetached), Do([&]() {
        glic::GlicProfileManager::GetInstance()->ShowProfilePicker();
      }),
      CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ClientUnresponsiveThenError) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicClientHangButton, true),
      ObserveState(test::internal::kGlicAppState, &host()),
      WaitForState(test::internal::kGlicAppState,
                   mojom::WebUiState::kUnresponsive),
      // Client should show error after showing the unresponsive UI for 5s.
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kError));
  histogram_tester.ExpectTotalCount(
      "Glic.Host.WebClientUnresponsiveState.Duration", 1);
  histogram_tester.ExpectTotalCount("Glic.Host.WebClientUnresponsiveState", 2);
  // One sample in the WebClientUnresponsiveState.ENTERED_FROM_CUSTOM_HEARTBEAT
  // (1) bucket.
  histogram_tester.ExpectBucketCount("Glic.Host.WebClientUnresponsiveState", 1,
                                     1);
  // One sample in the WebClientUnresponsiveState.EXITED (4) bucket.
  histogram_tester.ExpectBucketCount("Glic.Host.WebClientUnresponsiveState", 4,
                                     1);
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       InvalidatedAccountWhileLoadingGlic) {
  RunTestSequence(
      ObserveState(test::internal::kGlicAppState, &host()),
      SimulateGlicHotkey(), CheckControllerHasWidget(true),
      ForceInvalidateAccount(), WaitForAndInstrumentGlic(kHostOnly),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kSignIn),
      InAnyContext(ClickElement(test::kGlicHostElementId, {"#signInButton"},
                                ui_controls::LEFT, ui_controls::kNoAccelerator,
                                ExecuteJsMode::kFireAndForget)),
      WaitForHide(test::kGlicHostElementId),
      // Without a pause here, we will 'sign-in' before the callback is
      // registered to listen for it. This isn't a bug because it takes real
      // users finite time to actually sign-in.
      Wait(base::Milliseconds(500)), ForceReauthAccount(),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       InvalidatedAccountSignInOnGlicOpenFlow) {
  RunTestSequence(
      ObserveState(test::internal::kGlicAppState, &host()),
      ForceInvalidateAccount(), SimulateGlicHotkey(),
      CheckControllerHasWidget(false), InstrumentTab(kFirstTab),
      WaitForWebContentsReady(kFirstTab),
      // Without a pause here, we will 'sign-in' before the callback is
      // registered to listen for it. This isn't a bug because it takes real
      // users finite time to actually sign-in.
      Wait(base::Milliseconds(500)), ForceReauthAccount(),
      WaitForAndInstrumentGlic(kHostOnly),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       AccountInvalidatedWhileGlicOpen) {
  RunTestSequence(
      SimulateGlicHotkey(), CheckControllerHasWidget(true),
      ObserveState(test::internal::kGlicAppState, &host()),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady),
      ForceInvalidateAccount(),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kSignIn),
      ForceReauthAccount(),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, TestInitialBounds) {
  // The GlicButton and Tabstrip are not actually shown until a tab is created.
  chrome::AddTabAt(browser(), GURL("about:blank"), 0, true);
  // Calculate default location offset from work area.
  gfx::Point top_right =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().top_right();
  int expected_x = top_right.x() - GlicWidget::GetInitialSize().width() -
                   glic::kDefaultDetachedTopRightDistance;
  int expected_y = top_right.y() + glic::kDefaultDetachedTopRightDistance;
  gfx::Point default_origin(expected_x, expected_y);

  // Check that with no saved position the default location is used.
  gfx::Rect initial_bounds = window_controller().GetInitialBounds(nullptr);
  EXPECT_EQ(initial_bounds.origin(), default_origin);

  // Initial bounds with browser are valid and not default location.
  initial_bounds = window_controller().GetInitialBounds(browser());
  EXPECT_NE(initial_bounds.origin(), default_origin);

  // Use default location if Glic button location results in an invalid widget
  // location. Move browser window so that it is mostly off the screen to the
  // right.
  browser()->window()->SetBounds(
      {{top_right.x() + 500, top_right.y() + 50}, {900, 900}});
  initial_bounds = window_controller().GetInitialBounds(browser());
  EXPECT_EQ(initial_bounds.origin(), default_origin);

  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  struct TestPair {
    gfx::Point test;
    gfx::Point expected;
    std::string msg;
  };

  std::vector<TestPair> test_points = {
      {{10, 20}, {10, 20}, "Valid position on screen"},

      // Valid positions off each corner.
      {{-20, -2}, {-20, -2}, "Valid top-left"},
      {{-20, screen_bounds.height() - 100},
       {-20, screen_bounds.height() - 100},
       "Valid bottom left"},
      {{screen_bounds.width() - initial_bounds.width() + 20,
        screen_bounds.height() - 100},
       {screen_bounds.width() - initial_bounds.width() + 20,
        screen_bounds.height() - 100},
       "Valid bottom right"},
      {{screen_bounds.width() - initial_bounds.width() + 20, -2},
       {screen_bounds.width() - initial_bounds.width() + 20, -2},
       "Valid top right"},

      // Invalid positions off of each edge
      {{10, -5}, default_origin, "Invalid top"},
      {{-400, 10}, default_origin, "Invalid left"},
      {{10, screen_bounds.height() + 600}, default_origin, "Invalid bottom"},
      {{screen_bounds.width() + 400, 10}, default_origin, "Invalid right"},
  };

  for (auto& t : test_points) {
    window_controller().SetPreviousPositionForTesting(t.test);
    initial_bounds = window_controller().GetInitialBounds(nullptr);
    EXPECT_EQ(initial_bounds.origin(), t.expected) << t.msg;
  }
}

class GlicWindowControllerWithPreviousPostionUiTest
    : public GlicWindowControllerUiTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    // Set initial bounds via pref and check that they are used.
    Profile::FromBrowserContext(context)->GetPrefs()->SetInteger(
        prefs::kGlicPreviousPositionX, 20);
    Profile::FromBrowserContext(context)->GetPrefs()->SetInteger(
        prefs::kGlicPreviousPositionY, 10);
    test::InteractiveGlicTest::SetUpBrowserContextKeyedServices(context);
  }
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, PermanentlyDeleteProfile) {
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  Browser* const browser1 = CreateBrowser(&profile1);
  SigninWithPrimaryAccount(&profile1);
  SetModelExecutionCapability(&profile1, true);
  GlicKeyedService* const service1 =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser1->profile());
  service1->window_controller().fre_controller()->AcceptFre();
  EXPECT_TRUE(service1->enabling().HasConsented());

  // Open glic
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  service1->ToggleUI(nullptr, false, mojom::InvocationSource::kOsHotkey);
  EXPECT_TRUE(service1->IsWindowShowing());

  // Delete the second profile
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      browser1->profile()->GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  ui_test_utils::WaitForBrowserToClose(browser1);

  EXPECT_FALSE(service1->IsWindowShowing());
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerWithPreviousPostionUiTest,
                       TestInitialBounds) {
  // Check that the saved initial bounds are used.
  gfx::Rect initial_bounds = window_controller().GetInitialBounds(nullptr);
  ASSERT_EQ(initial_bounds.origin(), gfx::Point(20, 10));
}

class GlicWindowControllerUnloadOnCloseTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerUnloadOnCloseTest() {
    features_.InitAndEnableFeature(features::kGlicUnloadOnClose);
  }
  ~GlicWindowControllerUnloadOnCloseTest() override = default;

  auto CheckWebUiContentsExist(bool exist) {
    return CheckResult(
        [this]() { return !!glic_service()->host().webui_contents(); }, exist,
        "CheckWebUiContentsExist");
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUnloadOnCloseTest, UnloadOnClose) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(true), CheckWebUiContentsExist(true),
                  CloseGlicWindow(), CheckWebUiContentsExist(false));
}

class GlicWindowControllerWithMemoryPressureUiTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerWithMemoryPressureUiTest() {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicWarming,
          {{features::kGlicWarmingDelayMs.name, "0"},
           {features::kGlicWarmingJitterMs.name, "0"}}}},
        /*disabled_features=*/{});
  }
  ~GlicWindowControllerWithMemoryPressureUiTest() override = default;

  void SetUp() override {
    // This will temporarily disable preloading to ensure that we don't load the
    // web client before we've initialized the embedded test server and can set
    // the correct URL.
    GlicProfileManager::ForceMemoryPressureForTesting(
        base::MemoryPressureMonitor::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
    GlicWindowControllerUiTest::SetUp();
  }

  void TearDown() override {
    GlicWindowControllerUiTest::TearDown();
    GlicProfileManager::ForceMemoryPressureForTesting(std::nullopt);
  }

 protected:
  auto ResetMemoryPressure() {
    return Do([]() {
      GlicProfileManager::ForceMemoryPressureForTesting(
          base::MemoryPressureMonitor::MemoryPressureLevel::
              MEMORY_PRESSURE_LEVEL_NONE);
    });
  }

  auto TryPreload() {
    return Do([this]() { glic_service()->TryPreload(); });
  }

  auto CheckWarmed() {
    return Do([this]() { EXPECT_TRUE(window_controller().IsWarmed()); });
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerWithMemoryPressureUiTest, Preload) {
  // TODO(crbug.com/411100559): Wait for preload completion rather than assuming
  // that it will finish before the next step in the sequence.
  RunTestSequence(
      ResetMemoryPressure(), TryPreload(), CheckWarmed(),
      PressButton(kGlicButtonElementId),
      InAnyContext(
          WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)));
}

// These tests for dragging across multiple displays is for mac-only.
// On Windows11, this test times out in calling WaitForDisplaySizes() when
// setting up the virtual displays.
#if BUILDFLAG(IS_MAC)
class GlicWindowControllerMultipleDisplaysUiTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerMultipleDisplaysUiTest() = default;
  GlicWindowControllerMultipleDisplaysUiTest(
      const GlicWindowControllerMultipleDisplaysUiTest&) = delete;
  GlicWindowControllerMultipleDisplaysUiTest& operator=(
      const GlicWindowControllerMultipleDisplaysUiTest&) = delete;
  ~GlicWindowControllerMultipleDisplaysUiTest() override = default;

  // Create virtual displays as needed, ensuring 2 displays are available for
  // testing multi-screen functionality.
  bool SetUpVirtualDisplays() {
    if (display::Screen::GetScreen()->GetNumDisplays() > 1) {
      return true;
    }
    if ((virtual_display_util_ = display::test::VirtualDisplayUtil::TryCreate(
             display::Screen::GetScreen()))) {
      virtual_display_util_->AddDisplay(
          display::test::VirtualDisplayUtil::k1024x768);
      return true;
    }
    return false;
  }

  auto CheckDisplaysSetUp(bool is_set_up) {
    return CheckResult([this]() { return SetPrimaryAndSecondaryDisplay(); },
                       is_set_up, "CheckDisplaysSetUp");
  }

  auto CheckWidgetMovedToSecondaryDisplay(bool expect_moved) {
    return CheckResult(
        [this]() {
          return secondary_display_.id() ==
                 window_controller().GetGlicWidget()->GetNearestDisplay()->id();
        },
        expect_moved, "CheckWidgetMovedToSecondaryDisplay");
  }

  bool SetPrimaryAndSecondaryDisplay() {
    display::Display primary_display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    secondary_display_ =
        ui_test_utils::GetSecondaryDisplay(display::Screen::GetScreen());
    return primary_display.id() && secondary_display_.id();
  }

  auto MoveWidgetToSecondDisplay() {
    return Do([this]() {
      const gfx::Point target(secondary_display_.bounds().CenterPoint());
      // Replace with dragging simulation once supported.
      window_controller().GetGlicWidget()->SetBounds(
          gfx::Rect(target, window_controller()
                                .GetGlicWidget()
                                ->GetWindowBoundsInScreen()
                                .size()));
    });
  }

  auto DetachGlicWindow() {
    return Do([this]() { window_controller().Detach(); });
  }

  void TearDownOnMainThread() override {
    GlicWindowControllerUiTest::TearDownOnMainThread();
    virtual_display_util_.reset();
  }

 private:
  std::unique_ptr<display::test::VirtualDisplayUtil> virtual_display_util_;
  display::Display secondary_display_;
};

// TODO(crbug.com/399703468): Flaky on Mac. Test is targeted for Mac only.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerMultipleDisplaysUiTest,
                       DISABLED_MoveDetachedGlicWindowToSecondDisplay) {
  if (!SetUpVirtualDisplays()) {
    return;
  }

  RunTestSequence(CheckDisplaysSetUp(true),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  InAnyContext(MoveWidgetToSecondDisplay(),
                               CheckWidgetMovedToSecondaryDisplay(true)),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

// TODO(crbug.com/399703468): Flaky on Mac. Test is targeted for Mac only.
IN_PROC_BROWSER_TEST_F(
    GlicWindowControllerMultipleDisplaysUiTest,
    DISABLED_DetachAttachedGlicWindowAndMoveToSecondDisplay) {
  if (!SetUpVirtualDisplays()) {
    return;
  }

  RunTestSequence(CheckDisplaysSetUp(true),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  InAnyContext(DetachGlicWindow(), MoveWidgetToSecondDisplay(),
                               CheckWidgetMovedToSecondaryDisplay(true)));
}
#endif

}  // namespace glic
