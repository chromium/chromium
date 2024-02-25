// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/memory_pressure/container_oom_score_manager.h"

#include "ash/components/arc/mojom/process.mojom.h"  // For ProcessState.
#include "content/public/common/content_constants.h"  // For kLowestRendererOomScore.
#include "testing/gtest/include/gtest/gtest.h"        // For TEST.

namespace {

constexpr bool kIsFocused = true;
constexpr bool kNotFocused = false;

const int kMidOomScore =
    (content::kHighestRendererOomScore + content::kLowestRendererOomScore) / 2;

}  // namespace

TEST(ContainerOomScoreManager, AssignOomScoreAdjs) {
  // ContainerOomScoreManager uses the pid of ArcProcess, set different pid and
  // nspid in test to verify the nspid is not used by accident.
  constexpr base::ProcessId kNSPidPersistent1 = 1;
  constexpr base::ProcessId kNSPidPersistent2 = 2;
  constexpr base::ProcessId kNSPidFocused1 = 3;
  constexpr base::ProcessId kNSPidFocused2 = 4;
  constexpr base::ProcessId kNSPidVisible1 = 5;
  constexpr base::ProcessId kNSPidVisible2 = 6;
  constexpr base::ProcessId kNSPidVisible3 = 7;
  constexpr base::ProcessId kNSPidService1 = 8;
  constexpr base::ProcessId kNSPidService2 = 9;
  constexpr base::ProcessId kNSPidService3 = 10;
  constexpr base::ProcessId kNSPidService4 = 11;

  constexpr base::ProcessId kPidPersistent1 = 101;
  constexpr base::ProcessId kPidPersistent2 = 102;
  constexpr base::ProcessId kPidFocused1 = 103;
  constexpr base::ProcessId kPidFocused2 = 104;
  constexpr base::ProcessId kPidVisible1 = 105;
  constexpr base::ProcessId kPidVisible2 = 106;
  constexpr base::ProcessId kPidVisible3 = 107;
  constexpr base::ProcessId kPidService1 = 108;
  constexpr base::ProcessId kPidService2 = 109;
  constexpr base::ProcessId kPidService3 = 110;
  constexpr base::ProcessId kPidService4 = 111;

  std::vector<arc::ArcProcess> arc_processes;
  arc_processes.emplace_back(kNSPidVisible1, kPidVisible1,
                             /*process_name=*/"visible1",
                             arc::mojom::ProcessState::TOP, kNotFocused,
                             /*last_activity_time=*/6000);
  arc_processes.emplace_back(kNSPidService1, kPidService1, "service1",
                             arc::mojom::ProcessState::SERVICE, kNotFocused,
                             2000);
  arc_processes.emplace_back(kNSPidVisible2, kPidVisible2, "visible2",
                             arc::mojom::ProcessState::TOP, kNotFocused, 5000);
  arc_processes.emplace_back(kNSPidFocused1, kPidFocused1, "focused1",
                             arc::mojom::ProcessState::TOP, kIsFocused, 6000);
  arc_processes.emplace_back(kNSPidPersistent1, kPidPersistent1, "persistent1",
                             arc::mojom::ProcessState::PERSISTENT, kNotFocused,
                             6000);
  arc_processes.emplace_back(kNSPidVisible3, kPidVisible3, "visible3",
                             arc::mojom::ProcessState::TOP, kNotFocused, 5500);
  arc_processes.emplace_back(kNSPidFocused2, kPidFocused2, "focused2",
                             arc::mojom::ProcessState::TOP, kIsFocused, 3000);
  arc_processes.emplace_back(kNSPidPersistent2, kPidPersistent2, "persistent2",
                             arc::mojom::ProcessState::PERSISTENT, kNotFocused,
                             2000);
  arc_processes.emplace_back(kNSPidService2, kPidService2, "service2",
                             arc::mojom::ProcessState::SERVICE, kNotFocused,
                             3000);
  arc_processes.emplace_back(kNSPidService3, kPidService3, "service3",
                             arc::mojom::ProcessState::SERVICE, kNotFocused,
                             6000);
  arc_processes.emplace_back(kNSPidService4, kPidService4, "service4",
                             arc::mojom::ProcessState::SERVICE, kNotFocused,
                             5000);
  arc::ArcProcessService::OptionalArcProcessList opt_arc_processes(
      std::move(arc_processes));

  arc::ContainerOomScoreManager manager(true);

  manager.AssignOomScoreAdjs(std::move(opt_arc_processes));

  // persistent: kPidPersistent1, kPidPersistent2
  // focus: kPidFocused1, kPidFocused2
  // protected: kPidVisible1, kPidVisible3, kPidVisible2 (from low oom_score_adj
  // to high)
  // other: kPidService3, kPidService4, kPidService2, kPidService1 (from low
  // oom_score_adj to high)
  EXPECT_EQ(manager.GetCachedOomScore(kPidPersistent1),
            arc::ContainerOomScoreManager::kPersistentArcAppOomScore);
  EXPECT_EQ(manager.GetCachedOomScore(kPidPersistent2),
            arc::ContainerOomScoreManager::kPersistentArcAppOomScore);
  EXPECT_EQ(manager.GetCachedOomScore(kPidFocused1),
            arc::ContainerOomScoreManager::kFocusedArcAppOomScore);
  EXPECT_EQ(manager.GetCachedOomScore(kPidFocused2),
            arc::ContainerOomScoreManager::kFocusedArcAppOomScore);

  size_t protected_count = 3;
  float protected_increment =
      float(kMidOomScore - content::kLowestRendererOomScore) / protected_count;
  int32_t protected_score1 = content::kLowestRendererOomScore;
  int32_t protected_score2 =
      round(content::kLowestRendererOomScore + protected_increment);
  int32_t protected_score3 =
      round(content::kLowestRendererOomScore + protected_increment * 2);
  EXPECT_EQ(manager.GetCachedOomScore(kPidVisible1), protected_score1);
  EXPECT_EQ(manager.GetCachedOomScore(kPidVisible3), protected_score2);
  EXPECT_EQ(manager.GetCachedOomScore(kPidVisible2), protected_score3);

  size_t other_count = 4;
  float other_increment =
      float(content::kHighestRendererOomScore - kMidOomScore) / other_count;
  int32_t other_score1 = kMidOomScore;
  int32_t other_score2 = round(kMidOomScore + other_increment);
  int32_t other_score3 = round(kMidOomScore + other_increment * 2);
  int32_t other_score4 = round(kMidOomScore + other_increment * 3);
  EXPECT_EQ(manager.GetCachedOomScore(kPidService3), other_score1);
  EXPECT_EQ(manager.GetCachedOomScore(kPidService4), other_score2);
  EXPECT_EQ(manager.GetCachedOomScore(kPidService2), other_score3);
  EXPECT_EQ(manager.GetCachedOomScore(kPidService1), other_score4);
}
