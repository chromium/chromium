// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/memory_pressure/container_app_killer.h"

#include <map>
#include <vector>

#include "ash/components/arc/mojom/process.mojom.h"  // For ProcessState.
#include "base/process/process_handle.h"             // For ProcessId.
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"  // For TEST.

namespace {

constexpr bool kIsFocused = true;
constexpr bool kNotFocused = false;

}  // namespace

class ContainerAppKillerTest : public testing::Test {
 public:
  ContainerAppKillerTest() = default;
  ~ContainerAppKillerTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ContainerAppKillerTest, SortedCandidates) {
  std::vector<arc::ArcProcess> arc_processes;
  arc_processes.emplace_back(1, 20, "visible1", arc::mojom::ProcessState::TOP,
                             kNotFocused, 6000);
  arc_processes.emplace_back(
      2, 30, "service", arc::mojom::ProcessState::SERVICE, kNotFocused, 2001);
  arc_processes.emplace_back(3, 40, "visible2", arc::mojom::ProcessState::TOP,
                             kNotFocused, 5001);
  arc::ArcProcessService::OptionalArcProcessList opt_arc_processes(
      std::move(arc_processes));

  std::vector<arc::ContainerAppKiller::KillCandidate> candidates =
      arc::ContainerAppKiller::GetSortedCandidates(opt_arc_processes);
  EXPECT_EQ(3U, candidates.size());

  // Background service.
  EXPECT_EQ("service", candidates[0].process_name());
  // Visible app 2, last_activity_time less than visible app 1.
  EXPECT_EQ("visible2", candidates[1].process_name());
  // Visible app 1, last_activity_time larger than visible app 2.
  EXPECT_EQ("visible1", candidates[2].process_name());
}

class MockContainerAppKiller : public arc::ContainerAppKiller {
 public:
  void SetProcessRssKB(base::ProcessId pid, uint64_t rss_kb) {
    process_rss_kb_[pid] = rss_kb;
  }

  std::vector<base::ProcessId> GetKilledProcesses() {
    return killed_processes_;
  }

  bool IsRecentlyKilled(const std::string& process_name,
                        const base::TimeTicks& now) override {
    if (always_return_true_from_is_recently_killed_) {
      return true;
    }
    return arc::ContainerAppKiller::IsRecentlyKilled(process_name, now);
  }

  void set_always_return_true_from_is_recently_killed(
      bool always_return_true_from_is_recently_killed) {
    always_return_true_from_is_recently_killed_ =
        always_return_true_from_is_recently_killed;
  }

 protected:
  uint64_t GetMemoryFootprintKB(base::ProcessId pid) override {
    return process_rss_kb_[pid];
  }

  bool KillArcProcess(base::ProcessId nspid) override {
    killed_processes_.push_back(nspid);
    return true;
  }

 private:
  std::map<base::ProcessId, uint64_t> process_rss_kb_;
  std::vector<base::ProcessId> killed_processes_;
  bool always_return_true_from_is_recently_killed_ = false;
};

TEST_F(ContainerAppKillerTest, IsRecentlyKilled) {
  constexpr char kProcessName1[] = "org.chromium.arc.test1";
  constexpr char kProcessName2[] = "org.chromium.arc.test2";

  // Instantiate the mock instance.
  MockContainerAppKiller app_killer;

  // When the process name is not in the map, IsRecentlyKilled should
  // return false.
  const base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_FALSE(app_killer.IsRecentlyKilled(kProcessName1, now));
  EXPECT_FALSE(app_killer.IsRecentlyKilled(kProcessName2, now));

  // Update the map to tell the manager that the process was killed very
  // recently.
  app_killer.recently_killed_[kProcessName1] = now;
  EXPECT_TRUE(app_killer.IsRecentlyKilled(kProcessName1, now));
  EXPECT_FALSE(app_killer.IsRecentlyKilled(kProcessName2, now));
  app_killer.recently_killed_[kProcessName1] = now - base::Microseconds(1);
  EXPECT_TRUE(app_killer.IsRecentlyKilled(kProcessName1, now));
  EXPECT_FALSE(app_killer.IsRecentlyKilled(kProcessName2, now));
  app_killer.recently_killed_[kProcessName1] =
      now - arc::ContainerAppKiller::kArcRespawnKillDelay;
  EXPECT_TRUE(app_killer.IsRecentlyKilled(kProcessName1, now));
  EXPECT_FALSE(app_killer.IsRecentlyKilled(kProcessName2, now));

  // Update the map to tell the manager that the process was killed
  // (GetArcRespawnKillDelay() + 1) seconds ago. In this case,
  // IsRecentlyKilled(kProcessName1) should return false.
  app_killer.recently_killed_[kProcessName1] =
      now - arc::ContainerAppKiller::kArcRespawnKillDelay - base::Seconds(1);
  EXPECT_FALSE(app_killer.IsRecentlyKilled(kProcessName1, now));
  EXPECT_FALSE(app_killer.IsRecentlyKilled(kProcessName2, now));
}

TEST_F(ContainerAppKillerTest, DoNotKillRecentlyKilled) {
  // Instantiate the mock instance.
  MockContainerAppKiller app_killer;
  app_killer.set_always_return_true_from_is_recently_killed(true);

  std::vector<arc::ArcProcess> arc_processes;
  arc_processes.emplace_back(
      1, 10, "service", arc::mojom::ProcessState::SERVICE, kNotFocused, 500);

  app_killer.SetProcessRssKB(10, 10000);

  app_killer.LowMemoryKill(
      ash::ResourcedClient::PressureLevelArcContainer::kPerceptible, 250000,
      std::move(arc_processes));

  auto killed = app_killer.GetKilledProcesses();
  EXPECT_EQ(0U, killed.size());
}

TEST_F(ContainerAppKillerTest, KillMultipleProcesses) {
  // Instantiate the mock instance.
  MockContainerAppKiller app_killer;

  std::vector<arc::ArcProcess> arc_processes;
  arc_processes.emplace_back(1, 10, "focused", arc::mojom::ProcessState::TOP,
                             kIsFocused, 100);
  arc_processes.emplace_back(2, 20, "visible1", arc::mojom::ProcessState::TOP,
                             kNotFocused, 200);
  arc_processes.emplace_back(
      3, 30, "service", arc::mojom::ProcessState::SERVICE, kNotFocused, 500);
  arc_processes.emplace_back(4, 40, "visible2",
                             arc::mojom::ProcessState::IMPORTANT_FOREGROUND,
                             kNotFocused, 150);
  arc_processes.emplace_back(5, 50, "not-visible",
                             arc::mojom::ProcessState::IMPORTANT_BACKGROUND,
                             kNotFocused, 300);
  arc_processes.emplace_back(6, 60, "persistent",
                             arc::mojom::ProcessState::PERSISTENT, kNotFocused,
                             400);

  // Entities to be killed.
  app_killer.SetProcessRssKB(20, 30000);
  app_killer.SetProcessRssKB(30, 10000);
  app_killer.SetProcessRssKB(40, 50000);
  app_killer.SetProcessRssKB(50, 60000);
  // Should not be killed.
  app_killer.SetProcessRssKB(60, 500000);
  app_killer.SetProcessRssKB(10, 100000);

  app_killer.LowMemoryKill(
      ash::ResourcedClient::PressureLevelArcContainer::kPerceptible, 250000,
      std::move(arc_processes));

  auto killed_processes = app_killer.GetKilledProcesses();

  // Sorted order (by GetSortedCandidates, focused and persistent are excluded):
  // app "service"     pid: 30  nspid 3
  // app "visible2"    pid: 40  nspid 4
  // app "visible1"    pid: 20  nspid 2
  // app "not-visible" pid: 50  nspid 5

  // Killed processes and their nspid. All of the apps (except the focused and
  // persistent processes) should have been killed.
  EXPECT_EQ(4U, killed_processes.size());
  EXPECT_EQ(3, killed_processes[0]);
  EXPECT_EQ(4, killed_processes[1]);
  EXPECT_EQ(2, killed_processes[2]);
  EXPECT_EQ(5, killed_processes[3]);

  // Check that killed apps are in the map.
  const arc::ContainerAppKiller::KilledProcessesMap& processes_map =
      app_killer.recently_killed_;
  EXPECT_EQ(4U, processes_map.size());
  EXPECT_EQ(1U, processes_map.count("service"));
  EXPECT_EQ(1U, processes_map.count("not-visible"));
  EXPECT_EQ(1U, processes_map.count("visible1"));
  EXPECT_EQ(1U, processes_map.count("visible2"));
}
