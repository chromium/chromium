// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class SelectionOverlayBrowserTest : public GlicBrowserTest {
 public:
  SelectionOverlayBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(::features::kGlicCaptureRegion);
  }
  ~SelectionOverlayBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       SelectionUsedFromController) {
  base::HistogramTester histogram_tester;

  // 1. Navigate to a valid page.
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());

  // 2. Open Glic.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // 3. Show the selection overlay.
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr);

  // 4. Wait until state is State::kOverlay.
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  // 5. Adjust region.
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->AdjustRegion(
          selection::SelectedRegion::New(
              base::UnguessableToken::Create(),
              selection::RegionShape::NewRect(gfx::RectF(10, 10, 10, 10))),
          /*is_using_keyboard=*/false);

  // 6. Submit user input and verify metrics.
  SimulateUserInputSubmitted(instance, mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 1, 1);

  // Submit another input, should still log 1.
  SimulateUserInputSubmitted(instance, mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 1, 2);

  // Close the overlay.
  controller->Close();

  // Submit another input, should log 0.
  SimulateUserInputSubmitted(instance, mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 3);
}

}  // namespace glic
