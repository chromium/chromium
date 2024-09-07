// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/storage_manager_impl.h"

#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/metrics/structured/arena_event_buffer.h"
#include "chrome/browser/metrics/structured/event_logging_features.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics::structured {
namespace {
using ::google::protobuf::Arena;
using ::google::protobuf::RepeatedPtrField;

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
      flushed_map_(flush_dir, config_.disk_max_bytes),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  LogMaxMemorySizeKb(config_.buffer_max_bytes / 1024);
  LogMaxDiskSizeKb(config_.disk_max_bytes / 1024);

  event_buffer_ = std::make_unique<ArenaEventBuffer>(
      events_path,
      /*write_delay=*/base::Minutes(0), config_.buffer_max_bytes);
}

StorageManagerImpl::~StorageManagerImpl() = default;

void StorageManagerImpl::AddEvent(StructuredEventProto event) {
  const Result result = event_buffer_->AddEvent(event);
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

// Reads events from disk then from in-memory.
//
// This is a blocking operation since it could be reading events from disk. It
// would be best to put this in a task.
RepeatedPtrField<StructuredEventProto> StorageManagerImpl::TakeEvents() {
  RepeatedPtrField<StructuredEventProto> events;

  if (event_buffer_->Size() != 0) {
    TakeFromInMemory(&events);
  }

  if (!flushed_map_.empty()) {
    TakeFromDisk(&events);
  }
  return events;
}

// The implementation only says how many events are in-memory or if there are
// events storage on-disk.
//
// It would be to expensive to get the number of on-disk events to have an
// accurate value. I.E, if there are no events in-memory but there are some on
// disk, this still return > 0.
int StorageManagerImpl::RecordedEventsCount() const {
  uint64_t size = event_buffer_->Size();
  return size ? size : flushed_map_.keys().size();
}

void StorageManagerImpl::Purge() {
  event_buffer_->Purge();
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
      base::SysInfo::AmountOfFreeDiskSpace(base::FilePath(kRootPartitionPath));

  if (free_disk_space == -1) {
    free_disk_space = 0;
  }

  free_disk_space = GetMaxDiskSizeRatio() * free_disk_space;

  int64_t buffer_max_size =
      base::SysInfo::AmountOfPhysicalMemory() * GetMaxBufferSizeRatio();

  return StorageManagerConfig{
      .buffer_max_bytes = std::max(buffer_max_size, kMinBufferSize),
      .disk_max_bytes = std::max(free_disk_space, kMinDiskSize),
  };
}

void StorageManagerImpl::FlushAndAddEvent(StructuredEventProto&& event) {
  FlushBuffer();
  // Buffer should be cleared by this point.
  event_buffer_->AddEvent(event);
}

void StorageManagerImpl::FlushBuffer() {
  // The buffer is written to disk asynchronously, but it is cleared
  // synchonously and can be written to once this function returns.
  flushed_map_.Flush(
      *event_buffer_,
      base::BindPostTask(task_runner_,
                         base::BindOnce(&StorageManagerImpl::OnFlushCompleted,
                                        weak_factory_.GetWeakPtr())));
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
      if (dropping_flushed_queued_.load()) {
        break;
      }
      dropping_flushed_queued_.store(true);
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&StorageManagerImpl::DropFlushedUntilUnderQuota,
                         weak_factory_.GetWeakPtr()));
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

void StorageManagerImpl::TakeFromInMemory(
    RepeatedPtrField<StructuredEventProto>* output) {
  LogInMemoryEventsAtUpload(event_buffer_->Size());

  // Copy the events out of the buffer. We have to copy here because the events
  // stored in |events_buffer_| are allocated on an arena.
  output->MergeFrom(event_buffer_->Serialize());
  // Clear in-memory events and update the on-disk backup.
  event_buffer_->Purge();
}

void StorageManagerImpl::TakeFromDisk(
    RepeatedPtrField<StructuredEventProto>* output) {
  LogFlushedBuffersAtUpload(flushed_map_.keys().size());

  for (const auto& key : flushed_map_.keys()) {
    std::optional<EventsProto> events = flushed_map_.ReadKey(key);
    if (!events.has_value()) {
      continue;
    }

    // This is to avoid a deep copy of the |events| when using MergeFrom. This
    // should be more efficient despite the additional allocation.
    std::vector<StructuredEventProto*> elements(events->events_size(), nullptr);
    output->Reserve(output->size() + events->events_size());
    events->mutable_events()->ExtractSubrange(0, events->events_size(),
                                              elements.data());
    for (auto* event : elements) {
      output->AddAllocated(event);
    }
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StorageManagerImpl::CleanupFlushed,
                     weak_factory_.GetWeakPtr(), DeleteReason::kUploaded));
}

void StorageManagerImpl::CleanupFlushed(DeleteReason reason) {
  // Create copy of flushed keys no notify observers.
  std::vector<FlushedKey> keys = flushed_map_.keys();

  // All files are being deleted.
  flushed_map_.Purge();

  // Notify |delegate_| that the flushed files have been deleted.
  for (const auto& key : keys) {
    NotifyOnDeleted(key, reason);
  }
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

  for (const auto& key : dropped) {
    NotifyOnDeleted(key, DeleteReason::kExceededQuota);
  }

  dropping_flushed_queued_.store(false);
}

}  // namespace metrics::structured
