// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_event_storage.h"

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

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
    ino64_t inode,
    const DlpFileDestination& dst) {
  if (entries_num_ == entries_num_limit_) {
    // If we end up here we probably have already spammed the server with a lot
    // of events, better to stop for a while.
    return false;
  }

  const base::TimeTicks now = base::TimeTicks::Now();

  const auto inode_it = events_.find(inode);
  if (inode_it == events_.end()) {  // Check for new (inode, dst) pair
    InsertNewInodeAndDestinationPair(inode, dst, now);
    return true;
  }

  auto dst_it = inode_it->second.find(dst);
  if (dst_it == inode_it->second.end()) {  // Check for new dst for this inode
    AddDestinationToInode(inode_it, inode, dst, now);
    // Skip reporting if we don't know the destination (i.e., it is
    // kUnknownComponent) and at least an entry for `inode` is stored in
    // `events_`.
    return (dst.component().has_value() &&
            dst.component().value() !=
                DlpRulesManager::Component::kUnknownComponent) ||
           dst.url_or_path().has_value();
  }

  // Found existing (inode, dst) pair, update it
  UpdateInodeAndDestinationPair(dst_it, now);

  const auto time_diff = now - dst_it->second.timestamp;

  // Record the time difference between two identical file events.
  base::UmaHistogramTimes(
      GetDlpHistogramPrefix() + dlp::kSameFileEventTimeDiffUMA, time_diff);

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

void DlpFilesEventStorage::AddDestinationToInode(
    EventsMap::iterator inode_it,
    ino64_t inode,
    const DlpFileDestination& dst,
    const base::TimeTicks timestamp) {
  const auto [it, _] = inode_it->second.emplace(dst, timestamp);
  StartEvictionTimer(inode, dst, it->second);
  entries_num_++;
  DlpCountHistogram(dlp::kActiveFileEventsCount, entries_num_,
                    entries_num_limit_);
}

void DlpFilesEventStorage::InsertNewInodeAndDestinationPair(
    ino64_t inode,
    const DlpFileDestination& dst,
    const base::TimeTicks timestamp) {
  const auto [inode_it, _] =
      events_.emplace(inode, std::map<DlpFileDestination, EventEntry>());
  const auto [dst_it, __] = inode_it->second.emplace(dst, timestamp);
  StartEvictionTimer(inode, dst, dst_it->second);
  entries_num_++;
  DlpCountHistogram(dlp::kActiveFileEventsCount, entries_num_,
                    entries_num_limit_);
}

void DlpFilesEventStorage::UpdateInodeAndDestinationPair(
    DestinationsMap::iterator dst_it,
    const base::TimeTicks timestamp) {
  dst_it->second.timestamp = timestamp;
  DCHECK(dst_it->second.eviction_timer.IsRunning());
  dst_it->second.eviction_timer.Reset();
  DlpCountHistogram(dlp::kActiveFileEventsCount, entries_num_,
                    entries_num_limit_);
}

void DlpFilesEventStorage::StartEvictionTimer(ino64_t inode,
                                              const DlpFileDestination& dst,
                                              EventEntry& event_value) {
  event_value.eviction_timer.SetTaskRunner(task_runner_);
  event_value.eviction_timer.Start(
      FROM_HERE, cooldown_delta_,
      base::BindOnce(&DlpFilesEventStorage::OnEvictionTimerUp,
                     base::Unretained(this), inode, dst));
}

void DlpFilesEventStorage::OnEvictionTimerUp(ino64_t inode,
                                             DlpFileDestination dst) {
  auto event_it = events_.find(inode);
  DCHECK(event_it != events_.end());
  DCHECK(event_it->second.count(dst));
  event_it->second.erase(std::move(dst));
  if (event_it->second.empty()) {
    events_.erase(event_it);
  }
  --entries_num_;
}

}  // namespace policy
