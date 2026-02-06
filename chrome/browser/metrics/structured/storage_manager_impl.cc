// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/storage_manager_impl.h"

#include <vector>

#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics::structured {

namespace {

using ::google::protobuf::Arena;
using ::google::protobuf::RepeatedPtrField;

// Reads events from disk on a background thread.
StorageManagerImpl::FlushedEvents ReadEventsOnBackgroundThread(
    std::vector<FlushedKey> keys) {
  StorageManagerImpl::FlushedEvents result;
  for (const auto& key : keys) {
    if (auto events = FlushedMap::ReadKey(key)) {
      result.emplace_back(key, std::move(*events));
    }
  }
  return result;
}

// Default paths for Storage Manager on ChromeOS.
constexpr char kArenaProtoDefaultPath[] =
    "/var/lib/metrics/structured/chromium/storage/initial-events";

constexpr char kFlushedEventsDefaultDir[] =
    "/var/lib/metrics/structured/chromium/storage/flushed";

// Path of the partition used to store flushed events.
constexpr char kRootPartitionPath[] = "/var/lib/metrics/structured/";

// These minimum values are just guesses on what would be decent for low end
// devices.
constexpr int64_t kMinBufferSize = 10 * 1024;  // 10 kb
constexpr int64_t kMinDiskSize = 50 * 1024;    // 50 kb

}  // namespace

StorageManagerImpl::StorageManagerImpl(const StorageManagerConfig& config)
    : StorageManagerImpl(config,
                         base::FilePath(kArenaProtoDefaultPath),
                         base::FilePath(kFlushedEventsDefaultDir)) {}

StorageManagerImpl::StorageManagerImpl(const StorageManagerConfig& config,
                                       const base::FilePath& events_path,
                                       const base::FilePath& flush_dir)
    : config_(config),
      event_buffer_(events_path,
                    /*write_delay=*/base::Minutes(0),
                    config_.buffer_max_bytes),
      flushed_map_(flush_dir, config_.disk_max_bytes) {
  LogMaxMemorySizeKb(config_.buffer_max_bytes / 1024);
  LogMaxDiskSizeKb(config_.disk_max_bytes / 1024);
}

StorageManagerImpl::~StorageManagerImpl() = default;

void StorageManagerImpl::AddEvent(StructuredEventProto event) {
  const Result result = event_buffer_.AddEvent(event);
  // By default we assume it was successful, it is only an error if the result
  // is an kError.
  RecordStatus status;
  switch (result) {
    case Result::kOk:
      // Event added successfully.
      status = RecordStatus::kOk;
      break;
    case Result::kShouldFlush:
      // Flush the buffer.
      FlushBuffer();
      status = RecordStatus::kFlushed;
      break;
    case Result::kFull:
      // Flush the buffer then add event again.
      FlushAndAddEvent(std::move(event));
      status = RecordStatus::kFull;
      break;
    case Result::kError:
      status = RecordStatus::kError;
      break;
  }
  LogStorageManagerRecordStatus(status);
}

void StorageManagerImpl::TakeEvents(base::OnceCallback<void(Events)> consumer) {
  if (take_events_in_progress_) {
    std::move(consumer).Run(Events());
    return;
  }
  take_events_in_progress_ = true;

  Events in_memory_events;
  if (event_buffer_.Size() != 0) {
    TakeFromInMemory(&in_memory_events);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&ReadEventsOnBackgroundThread, flushed_map_.keys()),
      base::BindOnce(&StorageManagerImpl::CombineEventsOnUIThread,
                     weak_factory_.GetWeakPtr(), std::move(consumer),
                     std::move(in_memory_events)));
}

void StorageManagerImpl::CombineEventsOnUIThread(
    base::OnceCallback<void(Events)> consumer,
    Events in_memory_events,
    FlushedEvents disk_events) {
  if (!disk_events.empty()) {
    AddEventsFromDisk(std::move(disk_events), &in_memory_events);
  }
  std::move(consumer).Run(std::move(in_memory_events));
  take_events_in_progress_ = false;
}

// The implementation only says how many events are in-memory or if there are
// events storage on-disk.
//
// It would be to expensive to get the number of on-disk events to have an
// accurate value. I.E, if there are no events in-memory but there are some on
// disk, this still return > 0.
int StorageManagerImpl::RecordedEventsCount() const {
  uint64_t size = event_buffer_.Size();
  return size ? size : flushed_map_.keys().size();
}

void StorageManagerImpl::Purge() {
  event_buffer_.Purge();
  flushed_map_.Purge();
}

void StorageManagerImpl::AddBatchEvents(
    const RepeatedPtrField<StructuredEventProto>& events) {
  for (const auto& event : events) {
    AddEvent(event);
  }
}

// static
StorageManagerConfig StorageManagerImpl::GetStorageManagerConfig() {
  int64_t free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(base::FilePath(kRootPartitionPath))
          .value_or(0);

  free_disk_space = GetMaxDiskSizeRatio() * free_disk_space;

  int64_t buffer_max_size = base::SysInfo::AmountOfPhysicalMemory().InBytes() *
                            GetMaxBufferSizeRatio();

  return StorageManagerConfig{
      .buffer_max_bytes = std::max(buffer_max_size, kMinBufferSize),
      .disk_max_bytes = std::max(free_disk_space, kMinDiskSize),
  };
}

bool StorageManagerImpl::IsInitializedForTesting() const {
  return event_buffer_.IsInitialized() && flushed_map_.IsInitialized();
}

void StorageManagerImpl::FlushAndAddEvent(StructuredEventProto&& event) {
  FlushBuffer();
  // Buffer should be cleared by this point.
  event_buffer_.AddEvent(event);
}

void StorageManagerImpl::FlushBuffer() {
  // The buffer is written to disk asynchronously, but it is cleared
  // synchronously and can be written to once this function returns.
  flushed_map_.Flush(event_buffer_,
                     base::BindOnce(&StorageManagerImpl::OnFlushCompleted,
                                    weak_factory_.GetWeakPtr()));
}

void StorageManagerImpl::OnFlushCompleted(
    base::expected<FlushedKey, FlushError> key) {
  // If |key| has a value then the events were successfully written to disk.
  // Otherwise, there was an error while preparing the events or while writing
  // the file.
  if (key.has_value()) {
    LogStorageManagerFlushStatus(StorageManagerFlushStatus::kSuccessful);
    NotifyOnFlushed(*key);
    return;
  }

  switch (key.error()) {
    case FlushError::kQuotaExceeded:
      LogStorageManagerFlushStatus(StorageManagerFlushStatus::kQuotaExceeded);
      // The file is already flushed, just cleanup until we are under quota.
      DropFlushedUntilUnderQuota();
      break;
    // The write failed and we are unable to recover. The events that were being
    // written are unrecoverable and are lost.
    // TODO(b/342008451): Recover lost events when writing flushed events fails.
    case FlushError::kDiskFull:
      LogStorageManagerFlushStatus(StorageManagerFlushStatus::kDiskFull);
      break;
    case FlushError::kWriteError:
      LogStorageManagerFlushStatus(StorageManagerFlushStatus::kWriteError);
      break;
    case FlushError::kSerializationFailed:
      LogStorageManagerFlushStatus(
          StorageManagerFlushStatus::kEventSerializationError);
      break;
  }
}

void StorageManagerImpl::TakeFromInMemory(Events* output) {
  LogInMemoryEventsAtUpload(event_buffer_.Size());

  // Copy the events out of the buffer. We have to copy here because the events
  // stored in |events_buffer_| are allocated on an arena.
  output->MergeFrom(event_buffer_.Serialize());
  // Clear in-memory events and update the on-disk backup.
  event_buffer_.Purge();
}

void StorageManagerImpl::AddEventsFromDisk(FlushedEvents disk_events,
                                           Events* output) {
  LogFlushedBuffersAtUpload(disk_events.size());

  std::vector<FlushedKey> keys;
  keys.reserve(disk_events.size());

  for (auto& [key, events] : disk_events) {
    keys.push_back(key);
    // This is to avoid a deep copy of the |events| when using MergeFrom. This
    // should be more efficient despite the additional allocation.
    std::vector<StructuredEventProto*> elements(events.events_size(), nullptr);
    output->Reserve(output->size() + events.events_size());
    events.mutable_events()->ExtractSubrange(0, events.events_size(),
                                             elements.data());
    for (auto* event : elements) {
      output->AddAllocated(event);
    }
  }

  // Delete the events from disk now that they have been copied to |output|.
  flushed_map_.DeleteKeys(keys);
}

void StorageManagerImpl::DropFlushedUntilUnderQuota() {
  int64_t delta = flushed_map_.resource_info().used_size_bytes -
                  flushed_map_.resource_info().max_size_bytes;

  // Sanity check if flushed events need to be dropped.
  if (delta <= 0) {
    return;
  }

  LogDiskQuotaExceededDelta(delta / 1024);

  // The keys will be in the order of creation_time. Since we
  // are dropping by the oldest first, we can start from the
  // front of the keys list.
  std::vector<FlushedKey> dropped;
  for (const auto& key : flushed_map_.keys()) {
    if (delta <= 0) {
      break;
    }
    dropped.push_back(key);
    delta -= key.size;
  }

  LogDeletedBuffersWhenOverQuota(dropped.size());

  // Deletes the keys asynchronously.
  flushed_map_.DeleteKeys(dropped);
}

}  // namespace metrics::structured
