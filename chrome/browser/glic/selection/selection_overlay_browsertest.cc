// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace glic {

class SelectionOverlayBrowserTest : public NonInteractiveGlicTest {
 public:
  SelectionOverlayBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicCaptureRegion,
         // Only supports multi-instance mode for now.
         ::features::kGlicMultiInstance},
        {});
  }
  ~SelectionOverlayBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       SelectionUsedFromController) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(), CheckGlicInstanceIsShowing(),
      Do([this]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        auto* controller =
            SelectionOverlayController::FromTabWebContents(web_contents);
        controller->Show(/*options=*/nullptr);
      }),
      WaitUntil(
          [this]() -> std::string {
            content::WebContents* web_contents =
                browser()->tab_strip_model()->GetActiveWebContents();
            auto* controller =
                SelectionOverlayController::FromTabWebContents(web_contents);
            return (controller &&
                    controller->state() ==
                        SelectionOverlayController::State::kOverlay)
                       ? "overlay"
                       : "not overlay";
          },
          "overlay"),
      Do([this]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        auto* controller =
            SelectionOverlayController::FromTabWebContents(web_contents);
        static_cast<selection::SelectionOverlayPageHandler*>(controller)
            ->AdjustRegion(selection::SelectedRegion::New(
                base::UnguessableToken::Create(),
                selection::RegionShape::NewRect(gfx::RectF(10, 10, 10, 10))));
      }),
      Do([this, &histogram_tester]() {
        auto* host = GetHost();
        CHECK(host);

        host->instance_metrics_backwards_compatibility().OnUserInputSubmitted(
            mojom::WebClientMode::kText);
        histogram_tester.ExpectBucketCount(
            "Glic.Instance.InputSubmitted.SelectionCount", 1, 1);

        // Submit another input, should still log 1.
        host->instance_metrics_backwards_compatibility().OnUserInputSubmitted(
            mojom::WebClientMode::kText);
        histogram_tester.ExpectBucketCount(
            "Glic.Instance.InputSubmitted.SelectionCount", 1, 2);

        // Close the overlay.
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        SelectionOverlayController::FromTabWebContents(web_contents)->Close();

        // Submit another input, should log 0.
        host->instance_metrics_backwards_compatibility().OnUserInputSubmitted(
            mojom::WebClientMode::kText);
        histogram_tester.ExpectBucketCount(
            "Glic.Instance.InputSubmitted.SelectionCount", 0, 1);
        histogram_tester.ExpectTotalCount(
            "Glic.Instance.InputSubmitted.SelectionCount", 3);
      }),
      CloseGlic());
}

}  // namespace glic
