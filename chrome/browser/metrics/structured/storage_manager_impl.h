// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_STORAGE_MANAGER_IMPL_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_STORAGE_MANAGER_IMPL_H_

#include <atomic>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/metrics_scheduler.h"
#include "components/metrics/structured/flushed_map.h"
#include "components/metrics/structured/lib/event_buffer.h"
#include "components/metrics/structured/storage_manager.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

// Configuration data needed to create a Storage Manager.
struct StorageManagerConfig {
  // The maximum number of bytes the in-memory events can consume.
  int64_t buffer_max_bytes;
  // The maximum number of bytes the disk events can consume.
  int64_t disk_max_bytes;
};

// Provides a storage of events for Structured Metrics that tries to maintain a
// maximum usage of resources. This is done by in-memory and disk
// resources having a max size. In-memory resource get written to disk and disk
// resources are deleted once the quota is exceeded. This to be used only on
// Ash ChromeOS.
//
// There is no initialization. Events may be recorded immediately.
class StorageManagerImpl : public StorageManager {
 public:
  explicit StorageManagerImpl(const StorageManagerConfig& config);

  // Storage Manager Impl constructor
  // |config| contains how different components are configured.
  // |events_path| path event buffers will be temporarily storing events.
  // |flush_dir| the directory used to write event buffers to disk.
  StorageManagerImpl(const StorageManagerConfig& config,
                     const base::FilePath& events_path,
                     const base::FilePath& flush_dir);

  ~StorageManagerImpl() override;

  // EventStorage:
  void AddEvent(StructuredEventProto event) override;
  ::google::protobuf::RepeatedPtrField<StructuredEventProto> TakeEvents()
      override;
  int RecordedEventsCount() const override;
  void Purge() override;
  void AddBatchEvents(
      const google::protobuf::RepeatedPtrField<StructuredEventProto>& events)
      override;

  // Gets the configuration for the StorageManager using the configuration of
  // the system.
  static StorageManagerConfig GetStorageManagerConfig();

 private:
  // Flushes |event_buffer_| and then adds |event| to |event_buffer_| after.
  void FlushAndAddEvent(StructuredEventProto&& event);

  // Flush |event_buffer_| to disk.
  void FlushBuffer();

  // Handles Flush errors and propagates key to |service_|.
  void OnFlushCompleted(base::expected<FlushedKey, FlushError> key);

  // Retrieves events from the in-memory buffer.
  void TakeFromInMemory(
      google::protobuf::RepeatedPtrField<StructuredEventProto>* output);

  // Retrieves events from disk. All events are loaded.
  void TakeFromDisk(
      google::protobuf::RepeatedPtrField<StructuredEventProto>* output);

  // Cleans up all on-disk events, |storage_service_| is notified of the
  // deletion with |reason|.
  void CleanupFlushed(DeleteReason reason);

  // Deletes on-disk events from |flushed_map_| until it is under the desired
  // quota.
  void DropFlushedUntilUnderQuota();

  // The configuration used to create |this|.
  StorageManagerConfig config_;

  // An EventBuffer to store events in-memory.
  std::unique_ptr<EventBuffer<StructuredEventProto>> event_buffer_;
  // Manages flushed events.
  FlushedMap flushed_map_;

  // Flag denoting if flushed events are being dropped.
  std::atomic_bool dropping_flushed_queued_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<StorageManagerImpl> weak_factory_{this};
};
}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_STORAGE_MANAGER_IMPL_H_
