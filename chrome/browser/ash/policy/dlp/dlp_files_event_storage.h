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
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"

namespace policy {

// Stores file events and filters most non-user-initiated duplicate events.
class DlpFilesEventStorage {
 public:
  // File are identified by a pair of inode number and crtime
  // (creation time).
  typedef std::pair<ino64_t, time_t> FileId;

  DlpFilesEventStorage(base::TimeDelta cooldown_timeout, size_t entries_limit);
  DlpFilesEventStorage(const DlpFilesEventStorage& other) = delete;
  DlpFilesEventStorage(DlpFilesEventStorage&& other) = delete;
  ~DlpFilesEventStorage();

  // Upserts an event entry into `events_` and returns true if
  // enough time has passed from a previous call to
  // `StoreEventAndCheckIfItShouldBeReported` with the same `file_id` and `dst`.
  // When `dst` has `dst.component` equal to
  // `DlpRulesManager::Component::kUnknownComponent` and `dst.url_or_path` not
  // set, `StoreEventAndCheckIfItShouldBeReported` returns true only if no other
  // recent previous call has been performed with the same `file_id` and any
  // `dst`. Finally, it returns false also when the current number of entries
  // in `events_` is `entries_limit_`.
  bool StoreEventAndCheckIfItShouldBeReported(FileId file_id,
                                              const DlpFileDestination& dst);

  // Returns the time during which an event is filtered if an exactly similar
  // one has been already received.
  base::TimeDelta GetDeduplicationCooldownForTesting() const;

  // Returns the number of events that are no more than `cooldown_delta_` old.
  size_t GetSizeForTesting() const;

  // Used in tests to inject a task runner for time control.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  // Contains the information stored for every (file_id, destination) entry in
  // `events_`.
  struct EventEntry {
    explicit EventEntry(base::TimeTicks timestamp);
    ~EventEntry();
    // When the associated entry pair was upserted.
    base::TimeTicks timestamp;
    // Used to evict the entry when expired.
    base::OneShotTimer eviction_timer;
  };

  using DestinationsMap = std::map<DlpFileDestination, EventEntry>;
  using EventsMap = base::flat_map<FileId, DestinationsMap>;

  // Adds a new destination for an existing file_id.
  void AddDestinationToFile(EventsMap::iterator file_it,
                            FileId file_id,
                            const DlpFileDestination& dst,
                            const base::TimeTicks timestamp);

  // Inserts a new (file_id, destination) pair.
  void InsertNewFileAndDestinationPair(FileId file_id,
                                       const DlpFileDestination& dst,
                                       const base::TimeTicks timestamp);

  // Updates an existing (file_id, destination) pair.
  void UpdateFileAndDestinationPair(DestinationsMap::iterator dst_it,
                                    const base::TimeTicks timestamp);

  // Starts the eviction timer for an (file_id, destination) pair. When the
  // timer runs out of time, it calls `OnEvictionTimerUp`.
  void StartEvictionTimer(FileId file_id,
                          const DlpFileDestination& dst,
                          EventEntry& event_value);

  // Removes the given (file_id, destination) pair from `events_`.
  void OnEvictionTimerUp(FileId file_id, DlpFileDestination dst);

  EventsMap events_;

  const base::TimeDelta cooldown_delta_;

  // Used to evict entries when expired.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  size_t entries_num_ = 0;
  const size_t entries_num_limit_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_EVENT_STORAGE_H_
