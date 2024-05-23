// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_ARENA_EVENT_BUFFER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_ARENA_EVENT_BUFFER_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/metrics/structured/lib/arena_persistent_proto.h"
#include "components/metrics/structured/lib/event_buffer.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace base {
class FilePath;
}

namespace metrics::structured {

// An implementation of an EventBuffer that stored events in an
// ArenaPersistentProto.
//
// Since getting the in-memory size of the proto is not available in Chromium,
// an estimation is used. Events are serialized by copying the events into a
// RepeatedPtrField. This is necessary because the events are stored in an arena
// and the returned RepeatedPtrField isn't allocated from the same arena.
// Events are flushed by serializing the proto and writing it into the path
// provided.
class ArenaEventBuffer : public EventBuffer<StructuredEventProto> {
 public:
  ArenaEventBuffer(const base::FilePath& path,
                   base::TimeDelta write_delay,
                   int32_t max_size_bytes);

  ~ArenaEventBuffer() override;

  // EventBuffer:
  Result AddEvent(StructuredEventProto event) override;
  void Purge() override;
  uint64_t Size() override;
  google::protobuf::RepeatedPtrField<StructuredEventProto> Serialize() override;
  void Flush(const base::FilePath& path, FlushedCallback callback) override;

  // Updates the path of the persistent proto and merges the content of |path|
  // into |events_|.
  void UpdatePath(const base::FilePath& path);

  const google::protobuf::Arena* arena() const { return events_->arena(); }

  ArenaPersistentProto<EventsProto>& proto() { return *events_; }
  const ArenaPersistentProto<EventsProto>& proto() const { return *events_; }

  // Computes an estimate size in bytes of an event.
  //
  // The estimation is computed by summing:
  // * Size of StructuredEventProto
  // * Size of Metrics, times the number of metrics
  // * Size of event sequence metadata if it has one.
  static int32_t EstimateEventSize(const StructuredEventProto& event);

 private:
  void OnEventRead(const ReadStatus status);

  void OnEventWrite(const WriteStatus status);

  // Called periodically to backup |events_| to disk.
  void BackupTask();

  // The proto to store the events.
  std::unique_ptr<ArenaPersistentProto<EventsProto>> events_;

  // A timer to periodically backup |event_| to disk.
  base::RepeatingTimer backup_timer_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ArenaEventBuffer> weak_factory_{this};
};
}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_ARENA_EVENT_BUFFER_H_
