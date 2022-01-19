// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_indicator.h"

#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"

namespace ash {
namespace {

// TestHoldingSpaceProgressIndicator -------------------------------------------

class TestHoldingSpaceProgressIndicator : public HoldingSpaceProgressIndicator {
 public:
  TestHoldingSpaceProgressIndicator()
      : HoldingSpaceProgressIndicator(/*animation_registry=*/nullptr,
                                      /*animation_key=*/this) {}

  void SetProgress(const absl::optional<float>& progress) {
    progress_ = progress;
    static_cast<ui::LayerDelegate*>(this)->UpdateVisualState();
  }

 private:
  // HoldingSpaceProgressIndicator:
  absl::optional<float> CalculateProgress() const override { return progress_; }
  absl::optional<float> progress_;
};

}  // namespace

// HoldingSpaceProgressIndicatorTest -------------------------------------------

using HoldingSpaceProgressIndicatorTest = AshTestBase;

// Verifies that `HoldingSpaceProgressIndicator::AddProgressChangedCallback()`
// works as intended.
TEST_F(HoldingSpaceProgressIndicatorTest, AddProgressChangedCallback) {
  // Create a test `progress_indicator`.
  TestHoldingSpaceProgressIndicator progress_indicator;
  progress_indicator.SetProgress(0.5f);

  // Add a callback to be notified of progress changed events. The callback
  // should be invoked on progress changes so long as the returned subscription
  // continues to exist.
  int callback_call_count = 0;
  auto subscription =
      std::make_unique<base::RepeatingClosureList::Subscription>(
          progress_indicator.AddProgressChangedCallback(
              base::BindLambdaForTesting([&]() { ++callback_call_count; })));

  // Change the underlying progress.
  progress_indicator.SetProgress(0.75f);
  EXPECT_EQ(callback_call_count, 1);

  // Attempt to change the underlying progress to the same value.
  progress_indicator.SetProgress(0.75f);
  EXPECT_EQ(callback_call_count, 1);

  // Reset the subscription and change the underlying progress.
  subscription.reset();
  progress_indicator.SetProgress(1.f);
  EXPECT_EQ(callback_call_count, 1);
}

}  // namespace ash
