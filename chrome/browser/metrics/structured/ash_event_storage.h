// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_ASH_EVENT_STORAGE_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_ASH_EVENT_STORAGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/metrics/structured/event_storage.h"
#include "components/metrics/structured/persistent_proto.h"
#include "components/metrics/structured/storage.pb.h"

namespace metrics::structured {

// Storage for Structured Metrics events on Ash.
//
// Events are stored in a proto called EventsProto. It is persisted to disk in
// the user's cryptohome. This proto is not ready until a user has logged in.
// Events are stored in an in-memory vector until then.
class AshEventStorage : public EventStorage {
 public:
  explicit AshEventStorage(base::TimeDelta write_delay);

  ~AshEventStorage() override;

  // EventStorage:
  bool IsReady() override;
  void OnReady() override;
  void AddEvent(StructuredEventProto&& event) override;
  void MoveEvents(ChromeUserMetricsExtension& uma_proto) override;
  void Purge() override;
  void OnProfileAdded(const base::FilePath& path) override;
  void AddBatchEvents(
      const google::protobuf::RepeatedPtrField<StructuredEventProto>& events)
      override;

  EventsProto* events() { return events_->get(); }

 private:
  void OnWrite(const WriteStatus status);
  void OnRead(const ReadStatus status);

  bool is_initialized_ = false;

  // Delay period for PersistentProto writes. Default value of 1000 ms used if
  // not specified in ctor.
  base::TimeDelta write_delay_;

  // On-device storage within the user's cryptohome for unsent logs.
  std::unique_ptr<PersistentProto<EventsProto>> events_;

  // Storage for events to be stored if they are recorded before the storage is
  // ready. Should never be used once `OnReady` is called.
  std::vector<StructuredEventProto> pre_storage_events_;

  base::WeakPtrFactory<AshEventStorage> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_ASH_EVENT_STORAGE_H_
