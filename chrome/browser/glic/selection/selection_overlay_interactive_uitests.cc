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
      // The WebUI listens to the mouseUp event on the close button to dismiss
      // the overlay.
      MoveMouseTo(kOverlayWebContentsId,
                  {"selection-overlay-app", "#closeButton"}),
      ClickMouse(), WaitForHide(OverlayBaseController::kOverlayId));
}

}  // namespace glic
