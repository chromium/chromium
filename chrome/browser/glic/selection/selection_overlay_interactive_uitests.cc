// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ozone_buildflags.h"

namespace glic {

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

}  // namespace glic
