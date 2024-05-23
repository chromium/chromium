// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/arena_event_buffer.h"

#include <memory>
#include <streambuf>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/lib/histogram_util.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace metrics::structured {
namespace {
using google::protobuf::RepeatedPtrField;

uint64_t GetFreeDiskSpace(const base::FilePath& path) {
  if (int64_t size = base::SysInfo::AmountOfFreeDiskSpace(path); size != -1) {
    return static_cast<uint64_t>(size);
  }
  return 0;
}

base::expected<FlushedKey, FlushError> WriteEvents(const base::FilePath& path,
                                                   std::string content) {
  if (!base::WriteFile(path, content)) {
    if (GetFreeDiskSpace(path) < content.size()) {
      return base::unexpected(kDiskFull);
    }
    // Leaving proto content intact to let the caller handle cleanup.
    return base::unexpected(kWriteError);
  }

  base::File::Info info;
  CHECK(base::GetFileInfo(path, &info));

  return FlushedKey{
      .size = static_cast<int64_t>(content.size()),
      .path = path,
      .creation_time = info.creation_time,
  };
}
}  // namespace

ArenaEventBuffer::ArenaEventBuffer(const base::FilePath& path,
                                   base::TimeDelta write_delay,
                                   int32_t max_size_bytes)
    : EventBuffer(ResourceInfo(max_size_bytes)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  events_ = std::make_unique<ArenaPersistentProto<EventsProto>>(
      path, write_delay,
      base::BindOnce(&ArenaEventBuffer::OnEventRead,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&ArenaEventBuffer::OnEventWrite,
                          weak_factory_.GetWeakPtr()));
}

ArenaEventBuffer::~ArenaEventBuffer() = default;

Result ArenaEventBuffer::AddEvent(StructuredEventProto event) {
  const int32_t event_size = EstimateEventSize(event);

  if (!resource_info_.HasRoom(event_size)) {
    return Result::kFull;
  }

  (*events_)->mutable_events()->Add(std::move(event));

  resource_info_.Consume(event_size);

  // What would be a good heuristic here to determine if the buffer should
  // flush.
  // TODO(b/333938940): Investigate if using an event count is sufficient. If
  // so, then we can produce the ShouldFlush result.
  return Result::kOk;
}

void ArenaEventBuffer::Purge() {
  resource_info_.used_size_bytes = 0;
  events_->Purge();
}

uint64_t ArenaEventBuffer::Size() {
  return proto() ? proto()->events_size() : 0;
}

RepeatedPtrField<StructuredEventProto> ArenaEventBuffer::Serialize() {
  // Performance: performs a deep copy. Investigate an alternative to improve
  // performance.
  // TODO(b/339905988): Implement an optimization where two Persistent Protos
  // are used for staged and active that are swapped when a flush occurs.
  return proto()->events();
}

// This flushing for an ArenaEventBuffer will write the content
void ArenaEventBuffer::Flush(const base::FilePath& path,
                             FlushedCallback callback) {
  const base::FilePath proto_path = events_->path();

  std::string content;
  if (!proto()->SerializeToString(&content)) {
    std::move(callback).Run(base::unexpected(kSerializationFailed));
    return;
  }

  // Cleanup the in-memory events.
  Purge();

  // Write the events to disk. |callback| is expected to handle the key.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&WriteEvents, path, std::move(content)),
      std::move(callback));
}

void ArenaEventBuffer::UpdatePath(const base::FilePath& path) {
  events_->UpdatePath(path,
                      base::BindOnce(&ArenaEventBuffer::OnEventRead,
                                     weak_factory_.GetWeakPtr()),
                      /*remove_existing=*/true);
}

// static
int32_t ArenaEventBuffer::EstimateEventSize(const StructuredEventProto& event) {
  return sizeof(StructuredEventProto) +
         event.metrics_size() * sizeof(StructuredEventProto::Metric) +
         sizeof(StructuredEventProto) * event.has_event_sequence_metadata();
}

void ArenaEventBuffer::OnEventRead(const ReadStatus status) {
  switch (status) {
    case ReadStatus::kOk:
      // Update the used sized of the proto if a file was successfully loaded.
      resource_info_.used_size_bytes = (*events_)->ByteSizeLong();
      break;
    case ReadStatus::kMissing:
      break;
    case ReadStatus::kReadError:
      LogInternalError(StructuredMetricsError::kEventReadError);
      break;
    case ReadStatus::kParseError:
      LogInternalError(StructuredMetricsError::kEventParseError);
      break;
  }

  if (!backup_timer_.IsRunning()) {
    backup_timer_.Start(FROM_HERE, GetBackupTimeDelta(),
                        base::BindRepeating(&ArenaEventBuffer::BackupTask,
                                            weak_factory_.GetWeakPtr()));
  }
}

void ArenaEventBuffer::OnEventWrite(const WriteStatus status) {
  switch (status) {
    case WriteStatus::kOk:
      break;
    case WriteStatus::kWriteError:
      LogInternalError(StructuredMetricsError::kEventWriteError);
      break;
    case WriteStatus::kSerializationError:
      LogInternalError(StructuredMetricsError::kEventSerializationError);
      break;
  }
}

void ArenaEventBuffer::BackupTask() {
  // This task isn't started until after the OnReadComplete has been called so
  // we do not need to check if the proto has been created.
  events_->QueueWrite();
}

}  // namespace metrics::structured
