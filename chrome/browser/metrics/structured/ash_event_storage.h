// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_ASH_EVENT_STORAGE_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_ASH_EVENT_STORAGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/structured/profile_observer.h"
#include "components/metrics/structured/lib/event_storage.h"
#include "components/metrics/structured/lib/persistent_proto.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

// Storage for Structured Metrics events on Ash.
//
// Events are stored in a proto called EventsProto. These events are flushed to
// disk on a cadence. Before a user has logged in, these events will be stored
// in the shared partition. The events after a user has logged in, events will
// be stored in the user cryptohome.
class AshEventStorage : public EventStorage<StructuredEventProto>,
                        public ProfileObserver {
 public:
  // The delay period for the PersistentProto.
  constexpr static base::TimeDelta kSaveDelay = base::Seconds(1);

  AshEventStorage(base::TimeDelta write_delay,
                  const base::FilePath& pre_user_event_path);

  ~AshEventStorage() override;

  // EventStorage:
  void OnReady() override;
  void AddEvent(StructuredEventProto event) override;
  ::google::protobuf::RepeatedPtrField<StructuredEventProto> TakeEvents()
      override;
  int RecordedEventsCount() const override;
  void Purge() override;
  void AddBatchEvents(
      const google::protobuf::RepeatedPtrField<StructuredEventProto>& events)
      override;

  // ProfileObserver:
  void ProfileAdded(const Profile& profile) override;

  // Populates |proto| with a copy of the events currently recorded across both
  // |pre_user_events_| and |user_events_|.
  void CopyEvents(EventsProto* events_proto) const override;

 private:
  void OnWrite(const WriteStatus status);
  void OnRead(const ReadStatus status);
  void OnProfileRead(const ReadStatus status);

  EventsProto* pre_user_events() { return pre_user_events_->get(); }
  const EventsProto* pre_user_events() const { return pre_user_events_->get(); }

  EventsProto* user_events() { return user_events_->get(); }
  const EventsProto* user_events() const { return user_events_->get(); }

  // Retrieves the approproiate event store to write the event. Returns nullptr
  // if there is no appropriate place to persist the event.
  PersistentProto<EventsProto>* GetStoreToWriteEvent();

  // Callback to be made when profile event storage is ready to record.
  void OnProfileReady();

  bool IsProfileReady() const;
  bool IsPreUserStorageReadable() const;

  bool is_initialized_ = false;
  bool is_user_initialized_ = false;

  // Delay period for PersistentProto writes. Default value of 1000 ms used if
  // not specified in ctor.
  base::TimeDelta write_delay_;

  // Events captured before a user has logged in.
  std::unique_ptr<PersistentProto<EventsProto>> pre_user_events_;

  // On-device storage within the user's cryptohome for unsent logs.
  std::unique_ptr<PersistentProto<EventsProto>> user_events_;

  // Storage for events to be stored if they are recorded before the storage is
  // ready. Should never be used once `OnReady` is called.
  std::vector<StructuredEventProto> pre_storage_events_;

  base::WeakPtrFactory<AshEventStorage> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_ASH_EVENT_STORAGE_H_
