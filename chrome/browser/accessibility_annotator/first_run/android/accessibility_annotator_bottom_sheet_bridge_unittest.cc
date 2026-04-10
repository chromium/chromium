// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/android/accessibility_annotator_bottom_sheet_bridge.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using testing::_;

class AccessibilityAnnotatorBottomSheetBridgeTest : public testing::Test {
 public:
  AccessibilityAnnotatorBottomSheetBridgeTest() = default;
  ~AccessibilityAnnotatorBottomSheetBridgeTest() override = default;
};

TEST_F(AccessibilityAnnotatorBottomSheetBridgeTest,
       ShowWithoutJavaCallsCallbackNotAcknowledged) {
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  EXPECT_CALL(callback, Run(InfoResult::kNotAcknowledged));
  bridge->Show();
}

TEST_F(AccessibilityAnnotatorBottomSheetBridgeTest,
       OnInfoAcknowledgedRunsCallback) {
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  EXPECT_CALL(callback, Run(InfoResult::kAcknowledged));
  bridge->OnInfoAcknowledged(/*env=*/nullptr);
}

TEST_F(AccessibilityAnnotatorBottomSheetBridgeTest,
       OnInfoDismissedRunsCallback) {
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  EXPECT_CALL(callback, Run(InfoResult::kNotAcknowledged));
  bridge->OnInfoDismissed(/*env=*/nullptr);
}

TEST_F(AccessibilityAnnotatorBottomSheetBridgeTest,
       HideExecutesSafelyWithoutJavaRef) {
  base::MockCallback<base::OnceCallback<void(InfoResult)>> callback;
  auto bridge = std::make_unique<AccessibilityAnnotatorBottomSheetBridge>(
      /*web_contents=*/nullptr, callback.Get());

  bridge->Hide();
}

}  // namespace accessibility_annotator
