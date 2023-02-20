// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_swap_scheduler.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {
constexpr auto kSwapGap = base::Minutes(60);
constexpr auto kCheckingPreiod = base::Minutes(5);
}  // namespace

class ArcVmmSwapSchedulerTest : public testing::Test {
 public:
  ArcVmmSwapSchedulerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {
    ash::ConciergeClient::InitializeFake();
  }

  ArcVmmSwapSchedulerTest(const ArcVmmSwapSchedulerTest&) = delete;
  ArcVmmSwapSchedulerTest& operator=(const ArcVmmSwapSchedulerTest&) = delete;

  ~ArcVmmSwapSchedulerTest() override { ash::ConciergeClient::Shutdown(); }

  void InitScheduler(base::RepeatingCallback<bool()> swappable_checking_call,
                     base::RepeatingCallback<void(bool)> swap_call) {
    scheduler_ = std::make_unique<ArcVmmSwapScheduler>(
        kSwapGap, kCheckingPreiod, swappable_checking_call, swap_call);
  }

  void SetSwapOutTime(base::Time time) {
    local_state_.Get()->SetTime(prefs::kArcVmmSwapOutTime, base::Time());
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  ArcVmmSwapScheduler* scheduler() { return scheduler_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<ArcVmmSwapScheduler> scheduler_;

  ScopedTestingLocalState local_state_;
};

TEST_F(ArcVmmSwapSchedulerTest, FirstSwap) {
  // Pref value will be the base::Time() if it's empty.
  SetSwapOutTime(base::Time());

  int checking_count = 0, swap_count = 0;
  InitScheduler(base::BindLambdaForTesting([&]() {
                  checking_count++;
                  return true;
                }),
                base::BindLambdaForTesting([&](bool enabled) {
                  if (enabled) {
                    swap_count++;
                  }
                }));

  scheduler()->Start();
  base::RunLoop().RunUntilIdle();

  // Checking should wait for the first period.
  EXPECT_EQ(checking_count, 0);

  // Check and swapped.
  task_environment_.FastForwardBy(base::Minutes(20));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(checking_count, 1);
  EXPECT_EQ(swap_count, 1);
}
}  // namespace arc
