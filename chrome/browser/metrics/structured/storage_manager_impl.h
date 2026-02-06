// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_STORAGE_MANAGER_IMPL_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_STORAGE_MANAGER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/structured/arena_event_buffer.h"
#include "components/metrics/structured/flushed_map.h"
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
  using Events = ::google::protobuf::RepeatedPtrField<StructuredEventProto>;

  // Container for events read from disk.
  using FlushedEvents = std::vector<std::pair<FlushedKey, EventsProto>>;

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
  void TakeEvents(base::OnceCallback<void(Events)> consumer) override;
  int RecordedEventsCount() const override;
  void Purge() override;
  void AddBatchEvents(const Events& events) override;

  // Gets the configuration for the StorageManager using the configuration of
  // the system.
  static StorageManagerConfig GetStorageManagerConfig();

  bool IsInitializedForTesting() const;

 private:
  // Called on the UI thread to combine in-memory events and disk events.
  void CombineEventsOnUIThread(base::OnceCallback<void(Events)> consumer,
                               Events in_memory_events,
                               FlushedEvents disk_events);

  // Flushes |event_buffer_| and then adds |event| to |event_buffer_| after.
  void FlushAndAddEvent(StructuredEventProto&& event);

  // Flush |event_buffer_| to disk.
  void FlushBuffer();

  // Handles Flush errors and propagates key to |service_|.
  void OnFlushCompleted(base::expected<FlushedKey, FlushError> key);

  // Retrieves events from the in-memory buffer.
  void TakeFromInMemory(Events* output);

  // Adds events read from disk to |output| and cleans up the files.
  void AddEventsFromDisk(FlushedEvents disk_events, Events* output);

  // Deletes on-disk events from |flushed_map_| until it is under the desired
  // quota.
  void DropFlushedUntilUnderQuota();

  // The configuration used to create |this|.
  const StorageManagerConfig config_;

  // An EventBuffer to store events in-memory.
  ArenaEventBuffer event_buffer_;

  // Manages flushed events.
  FlushedMap flushed_map_;

  bool take_events_in_progress_ = false;

  base::WeakPtrFactory<StorageManagerImpl> weak_factory_{this};
};
}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_STORAGE_MANAGER_IMPL_H_
