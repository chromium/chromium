// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui_model.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keyboard {

TEST(KeyboardUIModelTest, ChangeToValidStateRecordsPositiveHistogram) {
  base::HistogramTester histogram_tester;

  KeyboardUIModel model;
  ASSERT_EQ(KeyboardUIState::kInitial, model.state());

  model.ChangeState(KeyboardUIState::kLoading);
  histogram_tester.ExpectUniqueSample(
      "VirtualKeyboard.ControllerStateTransition",
      GetStateTransitionHash(KeyboardUIState::kInitial,
                             KeyboardUIState::kLoading),
      1);
}

// Test fails DCHECK when the state transition is invalid. This is expected.
#if !DCHECK_IS_ON()
TEST(KeyboardUIModelTest, ChangeToInvalidStateRecordsNegativeHistogram) {
  base::HistogramTester histogram_tester;

  KeyboardUIModel model;
  ASSERT_EQ(KeyboardUIState::kInitial, model.state());

  model.ChangeState(KeyboardUIState::kShown);
  histogram_tester.ExpectUniqueSample(
      "VirtualKeyboard.ControllerStateTransition",
      -GetStateTransitionHash(KeyboardUIState::kInitial,
                              KeyboardUIState::kShown),
      1);
}
#endif

}  // namespace keyboard
