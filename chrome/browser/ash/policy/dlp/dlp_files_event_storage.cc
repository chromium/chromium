// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_event_storage.h"

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"

namespace policy {

DlpFilesEventStorage::DlpFilesEventStorage(base::TimeDelta cooldown_timeout,
                                           size_t entries_num_limit)
    : cooldown_delta_(cooldown_timeout),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      entries_num_limit_(entries_num_limit) {}
DlpFilesEventStorage::~DlpFilesEventStorage() = default;

DlpFilesEventStorage::EventEntry::EventEntry(base::TimeTicks timestamp)
    : timestamp(timestamp) {}
DlpFilesEventStorage::EventEntry::~EventEntry() = default;

bool DlpFilesEventStorage::StoreEventAndCheckIfItShouldBeReported(
    FileId file_id,
    const DlpFileDestination& dst) {
  if (entries_num_ == entries_num_limit_) {
    // If we end up here we probably have already spammed the server with a lot
    // of events, better to stop for a while.
    return false;
  }

  const base::TimeTicks now = base::TimeTicks::Now();

  const auto file_it = events_.find(file_id);
  if (file_it == events_.end()) {  // Check for new (file_id, dst) pair
    InsertNewFileAndDestinationPair(file_id, dst, now);
    return true;
  }

  auto dst_it = file_it->second.find(dst);
  if (dst_it == file_it->second.end()) {  // Check for new dst for this file_id
    AddDestinationToFile(file_it, file_id, dst, now);
    // Skip reporting if we don't know the destination (i.e., it is
    // kUnknownComponent) and at least an entry for `file_id` is stored in
    // `events_`.
    return (dst.component().has_value() &&
            dst.component().value() !=
                data_controls::Component::kUnknownComponent) ||
           dst.url().has_value();
  }

  // Found existing (file_id, dst) pair, update it
  UpdateFileAndDestinationPair(dst_it, now);

  const auto time_diff = now - dst_it->second.timestamp;

  // Record the time difference between two identical file events.
  base::UmaHistogramTimes(data_controls::GetDlpHistogramPrefix() +
                              data_controls::dlp::kSameFileEventTimeDiffUMA,
                          time_diff);

  // Report only if enough time has passed.
  return time_diff > cooldown_delta_;
}

base::TimeDelta DlpFilesEventStorage::GetDeduplicationCooldownForTesting()
    const {
  return cooldown_delta_;
}

std::size_t DlpFilesEventStorage::GetSizeForTesting() const {
  return entries_num_;
}

void DlpFilesEventStorage::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void DlpFilesEventStorage::AddDestinationToFile(
    EventsMap::iterator file_it,
    FileId file_id,
    const DlpFileDestination& dst,
    const base::TimeTicks timestamp) {
  const auto [it, _] = file_it->second.emplace(dst, timestamp);
  StartEvictionTimer(file_id, dst, it->second);
  entries_num_++;
  data_controls::DlpCountHistogram(data_controls::dlp::kActiveFileEventsCount,
                                   entries_num_, entries_num_limit_);
}

void DlpFilesEventStorage::InsertNewFileAndDestinationPair(
    FileId file_id,
    const DlpFileDestination& dst,
    const base::TimeTicks timestamp) {
  const auto [file_it, _] =
      events_.emplace(file_id, std::map<DlpFileDestination, EventEntry>());
  const auto [dst_it, __] = file_it->second.emplace(dst, timestamp);
  StartEvictionTimer(file_id, dst, dst_it->second);
  entries_num_++;
  data_controls::DlpCountHistogram(data_controls::dlp::kActiveFileEventsCount,
                                   entries_num_, entries_num_limit_);
}

void DlpFilesEventStorage::UpdateFileAndDestinationPair(
    DestinationsMap::iterator dst_it,
    const base::TimeTicks timestamp) {
  dst_it->second.timestamp = timestamp;
  DCHECK(dst_it->second.eviction_timer.IsRunning());
  dst_it->second.eviction_timer.Reset();
  data_controls::DlpCountHistogram(data_controls::dlp::kActiveFileEventsCount,
                                   entries_num_, entries_num_limit_);
}

void DlpFilesEventStorage::StartEvictionTimer(FileId file_id,
                                              const DlpFileDestination& dst,
                                              EventEntry& event_value) {
  event_value.eviction_timer.SetTaskRunner(task_runner_);
  event_value.eviction_timer.Start(
      FROM_HERE, cooldown_delta_,
      base::BindOnce(&DlpFilesEventStorage::OnEvictionTimerUp,
                     base::Unretained(this), file_id, dst));
}

void DlpFilesEventStorage::OnEvictionTimerUp(FileId file_id,
                                             DlpFileDestination dst) {
  auto event_it = events_.find(file_id);
  DCHECK(event_it != events_.end());
  DCHECK(event_it->second.count(dst));
  event_it->second.erase(std::move(dst));
  if (event_it->second.empty()) {
    events_.erase(event_it);
  }
  --entries_num_;
}

}  // namespace policy
