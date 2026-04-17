// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/interaction/element_tracker_views.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/shell.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace glic {

namespace {
auto GetPointWithOffset(int x, int y) {
  return base::BindLambdaForTesting([x, y](ui::TrackedElement* el) {
    auto* view = views::test::InteractiveViewsTestApi::AsView<views::View>(el);
    return view->GetBoundsInScreen().origin() + gfx::Vector2d(x, y);
  });
}
}  // namespace

class SelectionOverlayInteractiveTest : public test::InteractiveGlicTest {
 public:
  SelectionOverlayInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicRegionSelectionNew, ::features::kGlicCaptureRegion,
         // Only supports multi-instance mode for now.
         ::features::kGlicMultiInstance},
        {});
  }
  ~SelectionOverlayInteractiveTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, SmokeTest) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      // captureRegionBtn of the test client calls `captureRegion()` on the glic
      // API.
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // glic-selection-overlay is expected to be displayed.
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, MultiRegionSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};
  const DeepQuery kStaticRegion = {"selection-overlay-app",
                                   "glic-selection-overlay",
                                   "post-selection-renderer", ".static-region"};

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      // 0. Verify multi-region selection is enabled.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.multiRegionSelectionEnabled === true"),

      // 1. Draw first region (drag from 50,50 to 150,150).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      // Verify first region is active.
      WaitForJsResultAt(
          kOverlayWebContentsId, kRenderer,
          "el => el.hasSelection() && el.selectedRegions.length === 1"),

      // 2. Draw second region (starting far from the first one).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(300, 300)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(400, 400)),
      // Verify the newly added region which the mouse is on is the active
      // region.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      // Verify that the first region is now a static region and that there are
      // 2 total regions.
      WaitForElementVisible(kOverlayWebContentsId, kStaticRegion),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.staticRegions.length === 1 && "
                        "el.selectedRegions.length === 2"),

      // 3. Move mouse back over the first region (center is 100, 100).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(100, 100)),

      // Verify that hovering upon the first created region is now the active
      // region, and the second region becomes static.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      WaitForElementVisible(kOverlayWebContentsId, kStaticRegion),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.staticRegions.length === 1 && "
                        "el.selectedRegions.length === 2")

  );
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, DeleteActiveRegion) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};
  const DeepQuery kCloseButton = {"selection-overlay-app",
                                  "glic-selection-overlay",
                                  "post-selection-renderer", ".close-button"};
  const DeepQuery kStaticRegion = {"selection-overlay-app",
                                   "glic-selection-overlay",
                                   "post-selection-renderer", ".static-region"};

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      // 0. Verify multi-region selection is enabled.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.multiRegionSelectionEnabled === true"),

      // 1. Draw first region (drag from 50,50 to 150,150).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      // Verify first region is active.
      WaitForJsResultAt(
          kOverlayWebContentsId, kRenderer,
          "el => el.hasSelection() && el.selectedRegions.length === 1"),

      // 2. Draw second region (starting far from the first one).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(300, 300)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(400, 400)),
      // Verify the newly added region which the mouse is on is the active
      // region.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      // Verify that the first region is now a static region and that there are
      // 2 total regions.
      WaitForElementVisible(kOverlayWebContentsId, kStaticRegion),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.staticRegions.length === 1 && "
                        "el.selectedRegions.length === 2"),

      // 3. Move mouse back over the first region (center is 100, 100).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(100, 100)),

      // Verify that hovering upon the first created region is now the active
      // region, and the second region becomes static.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      WaitForElementVisible(kOverlayWebContentsId, kStaticRegion),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.staticRegions.length === 1 && "
                        "el.selectedRegions.length === 2"),
      // Click the close button for closing first region.
      ClickElement(kOverlayWebContentsId, kCloseButton),
      // Now move mouse to the first region.
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(350, 350)),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection() && "
                        "el.selectedRegions.length === 1"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, DeleteLastRegionClosesUI) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};
  const DeepQuery kCloseButton = {"selection-overlay-app",
                                  "glic-selection-overlay",
                                  "post-selection-renderer", ".close-button"};

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),

      // 1. Draw a region (drag from 50,50 to 150,150).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      // Verify this region is active.
      WaitForJsResultAt(
          kOverlayWebContentsId, kRenderer,
          "el => el.hasSelection() && el.selectedRegions.length === 1"),

      // 2. Move mouse back over the close button.
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(100, 100)),

      // 3. Move mouse directly to the close button.
      MoveMouseTo(kOverlayWebContentsId, kCloseButton),

      // 4. Click the mouse. This avoids failing if the element disappears immediately.
      ClickMouse(),

      // 5. Verify that the overlay is dismissed.
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       OverlayDismissedOnNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // glic-selection-overlay is expected to be displayed.
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      NavigateWebContents(kActiveTab,
                          embedded_test_server()->GetURL("/empty.html")),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, OverlayDismissedOnEsc) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // glic-selection-overlay is expected to be displayed.
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      SendKeyPress(OverlayBaseController::kOverlayId, ui::VKEY_ESCAPE),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       EscDismissesOverlayFirst) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // glic-selection-overlay is expected to be displayed.
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      FocusElement(test::kGlicContentsElementId),
      SendKeyPress(test::kGlicContentsElementId, ui::VKEY_ESCAPE),
      WaitForHide(OverlayBaseController::kOverlayId),
      EnsurePresent(test::kGlicHostElementId),
      SendKeyPress(test::kGlicContentsElementId, ui::VKEY_ESCAPE),
      WaitForHide(test::kGlicContentsElementId));
}

// When glic is in floating mode and when only the first tab has context shared,
// on a second tab, pressing esc in the floaty dismisses the floaty and
// therefore the selection overlay in the first tab.
//
// Fails on Wayland platforms and flaky on Mac.
#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND) || BUILDFLAG(IS_MAC)
#define MAYBE_EscDismissesFloatyOnSecondTab \
  DISABLED_EscDismissesFloatyOnSecondTab
#else
#define MAYBE_EscDismissesFloatyOnSecondTab EscDismissesFloatyOnSecondTab
#endif
IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       MAYBE_EscDismissesFloatyOnSecondTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAboutBlankTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kEmptyTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  TrackFloatingGlicInstance();
  RunTestSequence(
      InstrumentTab(kAboutBlankTab),
      OpenGlicFloatingWindow(GlicInstrumentMode::kHostAndContents,
                             /*conversation_id=*/std::nullopt),
      ClickMockGlicElement({"#pinFocusedTab"}),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      AddInstrumentedTab(kEmptyTab,
                         embedded_test_server()->GetURL("/empty.html")),
      FocusElement(kEmptyTab),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      InAnyContext(SendKeyPress(test::kGlicHostElementId, ui::VKEY_ESCAPE)),
      InAnyContext(WaitForHide(test::kGlicHostElementId)),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       FocusBackToGlicAfterSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kGlicHasFocus);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      CheckResult(
          [this]() {
            auto* instance = GetGlicInstanceImpl();
            return instance && instance->HasFocus();
          },
          false),
      PollState(kGlicHasFocus,
                [this]() {
                  auto* instance = GetGlicInstanceImpl();
                  return instance && instance->HasFocus();
                }),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(10, 10)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(100, 100)),
      WaitForState(kGlicHasFocus, true));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, BubbleUIColor) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      WaitForShow(lens::LensPreselectionBubble::kCancelButtonElementId),
      CheckView(kLensPreselectionBubbleElementId,
                [](views::View* view) {
                  auto* bubble =
                      static_cast<views::BubbleDialogDelegateView*>(view);
                  return bubble->background_color() ==
                         kColorGlicSelectionOverlayToast;
                }),
      CheckView(lens::LensPreselectionBubble::kCancelButtonElementId,
                [](views::View* view) {
                  auto* button = static_cast<views::MdTextButton*>(view);
                  return button->GetBgColorIdOverride() ==
                         kColorGlicSelectionOverlayToast;
                }));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, BubbleUICancelClicked) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      WaitForShow(lens::LensPreselectionBubble::kCancelButtonElementId),
      PressButton(lens::LensPreselectionBubble::kCancelButtonElementId),
      WaitForHide(OverlayBaseController::kOverlayId));
}
IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, BubbleUIIcon) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      CheckView(kLensPreselectionBubbleElementId, [](views::View* view) {
        auto* bubble = static_cast<views::BubbleDialogDelegateView*>(view);
        for (views::View* child : bubble->children()) {
          auto* image_view = views::AsViewClass<views::ImageView>(child);
          if (image_view) {
            const ui::ImageModel& model = image_view->GetImageModel();
            if (model.IsVectorIcon()) {
              return model.GetVectorIcon().vector_icon() ==
                     &vector_icons::kCropFreeIcon;
            }
          }
        }
        return false;
      }));
}

class SelectionOverlayHotkeyInteractiveTest
    : public SelectionOverlayInteractiveTest {
 public:
  SelectionOverlayHotkeyInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicDefaultTabContextSetting,
#if BUILDFLAG(IS_CHROMEOS)
         features::kGlicShowStatusTrayIcon,
         chromeos::features::kSupportCustomIconsInStatusArea
#endif
        },
        {});
  }
  ~SelectionOverlayHotkeyInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    SelectionOverlayInteractiveTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 true);
  }

  void TearDownOnMainThread() override {
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
    SelectionOverlayInteractiveTest::TearDownOnMainThread();
  }

  // Only call this on the browser UI thread.
  static bool IsHotkeySupported() {
    // ChromeOS uses ash's accelerator controller rather than global accelerator
    // listener.
#if BUILDFLAG(IS_CHROMEOS)
    if (ash::Shell::HasInstance()) {
      return ash::Shell::Get()->accelerator_controller() != nullptr;
    }
    return false;
#else
    auto* const global_shortcut_listener =
        ui::GlobalAcceleratorListener::GetInstance();
    return global_shortcut_listener != nullptr &&
           !global_shortcut_listener->IsRegistrationHandledExternally();
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_RequestCaptureRegionViaHotkey \
  DISABLED_RequestCaptureRegionViaHotkey
#else
#define MAYBE_RequestCaptureRegionViaHotkey \
  RequestCaptureRegionViaHotkey
#endif
IN_PROC_BROWSER_TEST_F(SelectionOverlayHotkeyInteractiveTest,
                       MAYBE_RequestCaptureRegionViaHotkey) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Hotkey not supported on the platform";
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      // SimulateAcceleratorPress() did not work.
      Do([]() {
        GlicBackgroundModeManager* const manager =
            g_browser_process->GetFeatures()->glic_background_mode_manager();
        manager->HandleHotkey(
            GlicLauncherConfiguration::GetDefaultSelectionHotkey());
      }),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}));
}

IN_PROC_BROWSER_TEST_F(
    SelectionOverlayHotkeyInteractiveTest,
    CannotRequestCaptureRegionViaHotkeyWithoutTabContextPermission) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Hotkey not supported on the platform";
  }
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  RunTestSequence(
      InstrumentTab(kActiveTab),
      // chrome://settings/'s context cannot be shared.
      NavigateWebContents(kActiveTab, GURL(chrome::kChromeUISettingsURL)),
      OpenGlic(),
      // SimulateAcceleratorPress() did not work.
      Do([]() {
        GlicBackgroundModeManager* const manager =
            g_browser_process->GetFeatures()->glic_background_mode_manager();
        manager->HandleHotkey(
            GlicLauncherConfiguration::GetDefaultSelectionHotkey());
      }),
      EnsureNotPresent(OverlayBaseController::kOverlayId));
}

}  // namespace glic
