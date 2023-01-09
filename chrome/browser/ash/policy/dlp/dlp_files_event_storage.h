// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_EVENT_STORAGE_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_EVENT_STORAGE_H_

#include <sys/types.h>
#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

namespace policy {

// Stores file events and filters most non-user-initiated duplicate events.
class DlpFilesEventStorage {
 public:
  DlpFilesEventStorage(base::TimeDelta cooldown_timeout, size_t entries_limit);
  DlpFilesEventStorage(const DlpFilesEventStorage& other) = delete;
  DlpFilesEventStorage(DlpFilesEventStorage&& other) = delete;
  ~DlpFilesEventStorage();

  // Upserts an event entry into `events_` and returns true if
  // enough time has passed from a previous call to
  // `StoreEventAndCheckIfItShouldBeReported` with the same `inode` and `dst`.
  // When `dst` has `dst.component` equal to
  // `DlpRulesManager::Component::kUnknownComponent` and `dst.url_or_path` not
  // set, `StoreEventAndCheckIfItShouldBeReported` returns true only if no other
  // recent previous call has been performed with the same `inode` and any
  // `dst`. Finally, it returns false also when the current number of entries
  // in `events_` is `entries_limit_`.
  bool StoreEventAndCheckIfItShouldBeReported(
      ino64_t inode,
      const DlpFilesController::DlpFileDestination& dst);

  // Returns the time during which an event is filtered if an exactly similar
  // one has been already received.
  base::TimeDelta GetDeduplicationCooldownForTesting() const;

  // Returns the number of events that are no more than `cooldown_delta_` old.
  size_t GetSizeForTesting() const;

  // Moves events timestamp back in time to simulate elapsed time.
  void SimulateElapsedTimeForTesting(base::TimeDelta time);

  // Used in tests to inject a task runner for time control.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  // Contains the information stored for every (inode, destination) entry in
  // `events_`.
  struct EventEntry {
    explicit EventEntry(base::TimeTicks timestamp);
    ~EventEntry();
    // When the associated entry pair was upserted.
    base::TimeTicks timestamp;
    // Used to evict the entry when expired.
    base::OneShotTimer eviction_timer;
  };

  using DestinationsMap =
      std::map<DlpFilesController::DlpFileDestination, EventEntry>;
  using EventsMap = base::flat_map<ino64_t, DestinationsMap>;

  // Adds a new destination for an existing inode.
  void AddDestinationToInode(EventsMap::iterator inode_it,
                             ino64_t inode,
                             const DlpFilesController::DlpFileDestination& dst,
                             const base::TimeTicks timestamp);

  // Inserts a new (inode, destination) pair.
  void InsertNewInodeAndDestinationPair(
      ino64_t inode,
      const DlpFilesController::DlpFileDestination& dst,
      const base::TimeTicks timestamp);

  // Updates an existing (inode, destination) pair.
  void UpdateInodeAndDestinationPair(DestinationsMap::iterator dst_it,
                                     const base::TimeTicks timestamp);

  // Starts the eviction timer for an (inode, destination) pair. When the timer
  // runs out of time, it calls `OnEvictionTimerUp`.
  void StartEvictionTimer(ino64_t inode,
                          const DlpFilesController::DlpFileDestination& dst,
                          EventEntry& event_value);

  // Removes the given (inode, destination) pair from `events_`.
  void OnEvictionTimerUp(ino64_t inode,
                         DlpFilesController::DlpFileDestination dst);

  EventsMap events_;

  const base::TimeDelta cooldown_delta_;

  // Used to evict entries when expired.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  size_t entries_num_ = 0;
  const size_t entries_num_limit_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_EVENT_STORAGE_H_
