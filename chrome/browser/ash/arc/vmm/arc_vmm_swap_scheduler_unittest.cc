// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_swap_scheduler.h"

#include <optional>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {
constexpr auto kCheckingPreiod = base::Minutes(5);

class TestPeaceDurationProvider : public PeaceDurationProvider {
 public:
  std::optional<base::TimeDelta> GetPeaceDuration() override {
    count_++;
    return duration_;
  }

  void SetDurationResetCallback(base::RepeatingClosure cb) override {
    reset_cb_ = std::move(cb);
  }

  void SetDuration(std::optional<base::TimeDelta> d) {
    duration_ = d;
    if (d == std::nullopt && !reset_cb_.is_null()) {
      reset_cb_.Run();
    }
  }
  int count() { return count_; }

 private:
  int count_ = 0;
  base::RepeatingClosure reset_cb_;
  std::optional<base::TimeDelta> duration_;
};

}  // namespace

class ArcVmmSwapSchedulerTest : public testing::Test {
 public:
  ArcVmmSwapSchedulerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {
    ash::ConciergeClient::InitializeFake();
  }

  ArcVmmSwapSchedulerTest(const ArcVmmSwapSchedulerTest&) = delete;
  ArcVmmSwapSchedulerTest& operator=(const ArcVmmSwapSchedulerTest&) = delete;

  ~ArcVmmSwapSchedulerTest() override { ash::ConciergeClient::Shutdown(); }

  void SetSwapOutTime(base::Time time) {
    local_state_.Get()->SetTime(prefs::kArcVmmSwapOutTime, base::Time());
  }

  base::Time GetSwapOutTime() {
    return local_state_.Get()->GetTime(prefs::kArcVmmSwapOutTime);
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  ScopedTestingLocalState local_state_;
};

TEST_F(ArcVmmSwapSchedulerTest, SetSwapEnableDisable) {
  int enable_count = 0, disable_count = 0;

  auto scheduler = std::make_unique<ArcVmmSwapScheduler>(
      base::BindLambdaForTesting([&](bool enabled) {
        if (enabled) {
          enable_count++;
        } else {
          disable_count++;
        }
      }),
      /* minimum_swapout_interval= */ std::nullopt,
      /* swappable_checking_period= */ std::nullopt, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(enable_count, 0);
  EXPECT_EQ(disable_count, 0);

  // Enable vmm swap.
  scheduler->SetSwappable(true);
  EXPECT_EQ(enable_count, 1);
  EXPECT_EQ(disable_count, 0);

  // Disable vmm swap.
  scheduler->SetSwappable(false);
  EXPECT_EQ(enable_count, 1);
  EXPECT_EQ(disable_count, 1);
}

TEST_F(ArcVmmSwapSchedulerTest, EnableSwap) {
  // Pref value will be the base::Time() if it's empty.
  SetSwapOutTime(base::Time());

  auto provider = std::make_unique<TestPeaceDurationProvider>();
  provider->SetDuration(kCheckingPreiod * 2);
  auto* provider_raw = provider.get();
  int swap_count = 0;

  auto scheduler = std::make_unique<ArcVmmSwapScheduler>(
      base::BindLambdaForTesting([&](bool enabled) {
        if (enabled) {
          swap_count++;
        }
      }),
      /* minimum_swapout_interval= */ std::nullopt,
      /* swappable_checking_period= */ kCheckingPreiod, std::move(provider));

  base::RunLoop().RunUntilIdle();

  // Checking should wait for the first period.
  EXPECT_EQ(provider_raw->count(), 0);

  // Check and swapped.
  task_environment_.FastForwardBy(base::Minutes(20));
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(provider_raw->count(), 0);
  EXPECT_GT(swap_count, 0);
}

TEST_F(ArcVmmSwapSchedulerTest, NeverEnableSwap) {
  // Pref value will be the base::Time() if it's empty.
  SetSwapOutTime(base::Time());

  auto provider = std::make_unique<TestPeaceDurationProvider>();
  provider->SetDuration(std::nullopt);
  auto* provider_raw = provider.get();
  int swap_count = 0;

  auto scheduler = std::make_unique<ArcVmmSwapScheduler>(
      base::BindLambdaForTesting([&](bool enabled) {
        if (enabled) {
          swap_count++;
        }
      }),
      /* minimum_swapout_interval= */ std::nullopt,
      /* swappable_checking_period= */ kCheckingPreiod, std::move(provider));

  base::RunLoop().RunUntilIdle();

  // Checking should wait for the first period.
  EXPECT_EQ(provider_raw->count(), 0);

  // Check and never swapped.
  task_environment_.FastForwardBy(base::Minutes(20));
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(provider_raw->count(), 0);
  EXPECT_EQ(swap_count, 0);
}

TEST_F(ArcVmmSwapSchedulerTest, EnableSwapAndDisableSwap) {
  // Pref value will be the base::Time() if it's empty.
  SetSwapOutTime(base::Time());

  auto provider = std::make_unique<TestPeaceDurationProvider>();
  provider->SetDuration(kCheckingPreiod * 2);
  auto* provider_raw = provider.get();
  int swap_count = 0;

  auto scheduler = std::make_unique<ArcVmmSwapScheduler>(
      base::BindLambdaForTesting([&](bool enabled) {
        if (enabled) {
          swap_count++;
        }
      }),
      /* minimum_swapout_interval= */ std::nullopt,
      /* swappable_checking_period= */ kCheckingPreiod, std::move(provider));

  base::RunLoop().RunUntilIdle();

  // Checking should wait for the first period.
  EXPECT_EQ(provider_raw->count(), 0);

  // Check and never swapped.
  task_environment_.FastForwardBy(base::Minutes(20));
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(provider_raw->count(), 0);
  EXPECT_GT(swap_count, 0);

  // Set system "busy" i.e. "not swappable".
  auto checking_count_before_busy = provider_raw->count();
  auto swap_count_before_busy = swap_count;
  provider_raw->SetDuration(std::nullopt);
  task_environment_.FastForwardBy(base::Minutes(20));
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(provider_raw->count(), checking_count_before_busy);
  EXPECT_EQ(swap_count, swap_count_before_busy);
}

TEST_F(ArcVmmSwapSchedulerTest, ReceiveSignalAndSave) {
  auto scheduler = std::make_unique<ArcVmmSwapScheduler>(
      base::NullCallback(),
      /* minimum_swapout_interval= */ std::nullopt,
      /* swappable_checking_period= */ std::nullopt, nullptr);

  SetSwapOutTime(base::Time());
  EXPECT_EQ(GetSwapOutTime(), base::Time());
  vm_tools::concierge::VmSwappingSignal signal;
  signal.set_name("arcvm");
  signal.set_state(vm_tools::concierge::SWAPPING_OUT);
  scheduler->OnVmSwapping(signal);

  EXPECT_NE(GetSwapOutTime(), base::Time());
}

TEST_F(ArcVmmSwapSchedulerTest, SetDisableVmStateWhenDurationReset) {
  // Pref value will be the base::Time() if it's empty.
  SetSwapOutTime(base::Time());

  auto provider = std::make_unique<TestPeaceDurationProvider>();
  provider->SetDuration(kCheckingPreiod * 2);

  int swap_count = 0;
  auto* provider_raw = provider.get();
  auto scheduler = std::make_unique<ArcVmmSwapScheduler>(
      base::BindLambdaForTesting([&](bool enabled) {
        if (enabled) {
          swap_count++;
        }
      }),
      /* minimum_swapout_interval= */ std::nullopt,
      /* swappable_checking_period= */ kCheckingPreiod, std::move(provider));

  provider_raw->SetDuration(base::Minutes(20));
  task_environment_.FastForwardBy(base::Minutes(20));
  base::RunLoop().RunUntilIdle();
  // Expect swap enabled.
  EXPECT_GT(swap_count, 0);

  task_environment_.FastForwardBy(base::Minutes(10));
  // Set ARC activated.
  provider_raw->SetDuration(std::nullopt);
  base::RunLoop().RunUntilIdle();
}

}  // namespace arc
