// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_power_controller.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/borealis/testing/widgets.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace borealis {
namespace {

class BorealisPowerControllerTest : public ChromeAshTestBase {
 protected:
  void SetUp() override {
    ChromeAshTestBase::SetUp();

    remote_provider_ =
        std::make_unique<mojo::Remote<device::mojom::WakeLockProvider>>();
    wake_lock_provider_ = std::make_unique<device::TestWakeLockProvider>();
    wake_lock_provider_->BindReceiver(
        remote_provider_->BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    wake_lock_provider_.reset();
    remote_provider_.reset();
    ChromeAshTestBase::TearDown();
  }

  // Returns the number of active wake locks.
  int CountActiveWakeLocks() {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_->GetActiveWakeLocksForTests(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_count, int32_t count) {
              *result_count = count;
              run_loop->Quit();
            },
            &run_loop, &result_count));
    run_loop.Run();
    return result_count;
  }

  // Flushes the outstanding messages on the wakelock and returns active wake
  // locks.
  int FlushAndCountWakeLocks(BorealisPowerController& power_controller) {
    power_controller.FlushForTesting();
    return CountActiveWakeLocks();
  }

  const std::string kBorealisPrefix = "org.chromium.borealis.";

  std::unique_ptr<mojo::Remote<device::mojom::WakeLockProvider>>
      remote_provider_;
  std::unique_ptr<device::TestWakeLockProvider> wake_lock_provider_;
};

TEST_F(BorealisPowerControllerTest, PreventSleepWhenBorealisWindowShown) {
  BorealisPowerController power_controller;
  power_controller.SetWakeLockProviderForTesting(std::move(*remote_provider_));
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);

  std::unique_ptr<views::Widget> borealis_widget =
      CreateFakeWidget(kBorealisPrefix + "w1");
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 1);

  borealis_widget->Hide();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);
}

TEST_F(BorealisPowerControllerTest, DoNothingWhenRegularWindowShown) {
  BorealisPowerController power_controller;
  power_controller.SetWakeLockProviderForTesting(std::move(*remote_provider_));
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);

  std::unique_ptr<views::Widget> not_borealis_widget =
      CreateFakeWidget("org.chromium.noborealis.w1");
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);

  not_borealis_widget->Hide();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);
}

TEST_F(BorealisPowerControllerTest, WakeLockToggledWhenWindowFocusChanges) {
  BorealisPowerController power_controller;
  power_controller.SetWakeLockProviderForTesting(std::move(*remote_provider_));
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);

  std::unique_ptr<views::Widget> borealis_widget =
      CreateFakeWidget(kBorealisPrefix + "w1");
  aura::Window* borealis_window = borealis_widget->GetNativeWindow();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 1);

  std::unique_ptr<views::Widget> second_borealis_widget =
      CreateFakeWidget(kBorealisPrefix + "w2");
  aura::Window* second_borealis_window =
      second_borealis_widget->GetNativeWindow();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 1);

  std::unique_ptr<views::Widget> not_borealis_widget =
      CreateFakeWidget("org.chromium.noborealis.w1");
  aura::Window* not_borealis_window = not_borealis_widget->GetNativeWindow();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);

  borealis_window->Focus();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 1);

  second_borealis_window->Focus();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 1);

  not_borealis_window->Focus();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);

  not_borealis_widget->Hide();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 1);

  second_borealis_widget->Hide();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 1);

  borealis_widget->Hide();
  EXPECT_EQ(FlushAndCountWakeLocks(power_controller), 0);
}

TEST_F(BorealisPowerControllerTest, WakeLockResetWhenBorealisShutsdown) {
  std::unique_ptr<BorealisPowerController> power_controller =
      std::make_unique<BorealisPowerController>();
  power_controller->SetWakeLockProviderForTesting(std::move(*remote_provider_));
  EXPECT_EQ(FlushAndCountWakeLocks(*power_controller), 0);

  std::unique_ptr<views::Widget> borealis_widget =
      CreateFakeWidget(kBorealisPrefix + "w1");
  EXPECT_EQ(FlushAndCountWakeLocks(*power_controller), 1);

  power_controller.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CountActiveWakeLocks(), 0);
}

}  // namespace
}  // namespace borealis
