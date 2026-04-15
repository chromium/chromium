// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/android/accessibility_annotator_bottom_sheet_bridge.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using testing::_;

class TestAccessibilityAnnotatorBottomSheetBridge
    : public AccessibilityAnnotatorBottomSheetBridge {
 public:
  using AccessibilityAnnotatorBottomSheetBridge::
      AccessibilityAnnotatorBottomSheetBridge;

  void SetShowResult(bool result) { show_result_ = result; }

 protected:
  bool PerformShowContent() override { return show_result_; }

 private:
  bool show_result_ = false;
};

TEST(AccessibilityAnnotatorBottomSheetBridgeTest, ShowWithoutJava) {
  base::HistogramTester histogram_tester;
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  EXPECT_CALL(callback, Run(InfoResult::kNotAcknowledged));
  bridge->Show();

  histogram_tester.ExpectTotalCount(
      "AccessibilityAnnotator.RemoteAnnotatorInfo", 0);
}

TEST(AccessibilityAnnotatorBottomSheetBridgeTest, ShowSuccessRecordsMetric) {
  base::HistogramTester histogram_tester;
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;

  auto bridge = std::make_unique<TestAccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  bridge->SetShowResult(true);
  bridge->Show();

  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.RemoteAnnotatorInfo",
      InfoShowRequestResult::kShown, 1);
}

TEST(AccessibilityAnnotatorBottomSheetBridgeTest, OnInfoAcknowledged) {
  base::HistogramTester histogram_tester;
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  EXPECT_CALL(callback, Run(InfoResult::kAcknowledged));
  bridge->OnInfoAcknowledged(/*env=*/nullptr);

  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.RemoteAnnotatorInfo",
      InfoShowRequestResult::kAccepted, 1);
}

TEST(AccessibilityAnnotatorBottomSheetBridgeTest, OnInfoDismissed) {
  base::HistogramTester histogram_tester;
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  EXPECT_CALL(callback, Run(InfoResult::kNotAcknowledged));
  bridge->OnInfoDismissed(/*env=*/nullptr);

  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.RemoteAnnotatorInfo",
      InfoShowRequestResult::kDismissed, 1);
}

TEST(AccessibilityAnnotatorBottomSheetBridgeTest, OnManageSettingsClicked) {
  base::UserActionTester user_action_tester;
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  bridge->OnManageSettingsClicked(/*env=*/nullptr);

  EXPECT_EQ(
      1, user_action_tester.GetActionCount(
             "AccessibilityAnnotator.RemoteAnnotatorInfo.SettingsLinkClick"));
}

TEST(AccessibilityAnnotatorBottomSheetBridgeTest, OnLearnMoreClicked) {
  base::UserActionTester user_action_tester;
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  bridge->OnLearnMoreClicked(/*env=*/nullptr);

  EXPECT_EQ(
      1, user_action_tester.GetActionCount(
             "AccessibilityAnnotator.RemoteAnnotatorInfo.LearnMoreLinkClick"));
}

TEST(AccessibilityAnnotatorBottomSheetBridgeTest, HideWithoutJava) {
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  bridge->Hide();
}

}  // namespace accessibility_annotator
