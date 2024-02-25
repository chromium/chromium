// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_MEMORY_PRESSURE_CONTAINER_OOM_SCORE_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_MEMORY_PRESSURE_CONTAINER_OOM_SCORE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"         // For FORWARD_DECLARE_TEST.
#include "base/memory/weak_ptr.h"         // For WeakPtrFactory.
#include "base/process/process_handle.h"  // For ProcessId.
#include "base/timer/timer.h"             // For RepeatingTimer.
#include "chrome/browser/ash/arc/process/arc_process_service.h"

FORWARD_DECLARE_TEST(ContainerOomScoreManager, AssignOomScoreAdjs);

namespace arc {

// Assigning oom score adj to ARC container processes. Process with lowest oom
// score adj is the last to be killed by Linux oom killer. The more important
// process would be assigned lower oom score adj. See the following web page for
// more explanation on Linux oom score adj(adjust).
// [1]: https://man7.org/linux/man-pages/man1/choom.1.html
class ContainerOomScoreManager {
 public:
  ContainerOomScoreManager();
  ContainerOomScoreManager(const ContainerOomScoreManager&) = delete;
  ContainerOomScoreManager& operator=(const ContainerOomScoreManager&) = delete;
  ~ContainerOomScoreManager();

 private:
  FRIEND_TEST_ALL_PREFIXES(::ContainerOomScoreManager, AssignOomScoreAdjs);

  static constexpr int32_t kPersistentArcAppOomScore = -100;
  static constexpr int32_t kFocusedArcAppOomScore = 0;

  // Caches OOM scores in memory to avoid setting oom_score_adj that is not
  // changed.
  using ProcessScoreMap = base::flat_map<base::ProcessId, int32_t>;

  // Special constructor for testing.
  explicit ContainerOomScoreManager(bool testing);

  // Assigns oom_score_adj. Higher priority process has lower oom_score_adj
  // (harder to be killed by the oom killer).
  void AssignOomScoreAdjs(
      ArcProcessService::OptionalArcProcessList arc_processes);

  // Returns the cached oom score adj. If the pid is not cached, returns -1 (a
  // value not in the valid oom score adj range for renderer processes).
  int32_t GetCachedOomScore(base::ProcessId pid);

  // Is called periodically to assign oom_score_adj.
  void OnTimer();

  // Set the oom_score_adj according to the difference between the new map and
  // the old map.
  void SetOomScoreAdjs(const ProcessScoreMap& new_map);

  // Is in unit test.
  bool testing_;

  // Map maintaining the process handle to oom_score_adj mapping.
  ProcessScoreMap oom_score_map_;

  // Periodically assigns oom_score_adj.
  base::RepeatingTimer timer_;

  base::WeakPtrFactory<ContainerOomScoreManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_MEMORY_PRESSURE_CONTAINER_OOM_SCORE_MANAGER_H_
