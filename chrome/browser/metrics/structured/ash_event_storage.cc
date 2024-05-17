// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_event_storage.h"

#include "base/functional/callback_forward.h"
#include "base/task/current_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "components/metrics/structured/lib/histogram_util.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

using ::google::protobuf::RepeatedPtrField;

AshEventStorage::AshEventStorage(base::TimeDelta write_delay,
                                 const base::FilePath& pre_user_event_path)
    : write_delay_(write_delay) {
  // Store to persist events before a user has logged-in.
  pre_user_events_ = std::make_unique<PersistentProto<EventsProto>>(
      pre_user_event_path, write_delay_,
      base::BindOnce(&AshEventStorage::OnRead, weak_factory_.GetWeakPtr()),
      base::BindRepeating(&AshEventStorage::OnWrite,
                          weak_factory_.GetWeakPtr()));
}

AshEventStorage::~AshEventStorage() = default;

void AshEventStorage::OnReady() {
  CHECK(pre_user_events_.get());
  is_initialized_ = true;

  for (auto& event : pre_storage_events_) {
    AddEvent(std::move(event));
  }
}

void AshEventStorage::AddEvent(StructuredEventProto event) {
  PersistentProto<EventsProto>* event_store_to_write = GetStoreToWriteEvent();

  if (!event_store_to_write) {
    pre_storage_events_.emplace_back(std::move(event));
    return;
  }

  event_store_to_write->get()->mutable_events()->Add(std::move(event));
  event_store_to_write->QueueWrite();
}

RepeatedPtrField<StructuredEventProto> AshEventStorage::TakeEvents() {
  if (IsPreUserStorageReadable()) {
    RepeatedPtrField<StructuredEventProto> events =
        std::move(*pre_user_events()->mutable_events());
    pre_user_events_->Purge();
    return events;
  }

  // Profile must be ready if |pre_user_events| has been cleanedup.
  CHECK(IsProfileReady());

  RepeatedPtrField<StructuredEventProto> events =
      std::move(*user_events()->mutable_events());
  user_events_->Purge();
  return events;
}

int AshEventStorage::RecordedEventsCount() const {
  int total_event_count = 0;
  if (IsPreUserStorageReadable()) {
    total_event_count += pre_user_events_->get()->events_size();
  }
  if (is_user_initialized_) {
    total_event_count += user_events_->get()->events_size();
  }
  return total_event_count;
}

void AshEventStorage::Purge() {
  if (IsProfileReady()) {
    user_events_->Purge();
  }
  if (IsPreUserStorageReadable()) {
    pre_user_events_->Purge();
  }
  if (!pre_storage_events_.empty()) {
    pre_storage_events_.clear();
  }
}

void AshEventStorage::AddBatchEvents(
    const google::protobuf::RepeatedPtrField<StructuredEventProto>& events) {
  PersistentProto<EventsProto>* event_store = GetStoreToWriteEvent();
  if (event_store) {
    event_store->get()->mutable_events()->MergeFrom(events);
    event_store->QueueWrite();
  } else if (!is_initialized_) {
    pre_storage_events_.insert(pre_storage_events_.end(), events.begin(),
                               events.end());
  }
}

void AshEventStorage::ProfileAdded(const Profile& profile) {
  DCHECK(base::CurrentUIThread::IsSet());

  if (is_user_initialized_) {
    return;
  }

  const base::FilePath& path = profile.GetPath();

  // The directory used to store unsent logs. Relative to the user's cryptohome.
  // This file is created by chromium.
  user_events_ = std::make_unique<PersistentProto<EventsProto>>(
      path.Append(FILE_PATH_LITERAL("structured_metrics"))
          .Append(FILE_PATH_LITERAL("events")),
      write_delay_,
      base::BindOnce(&AshEventStorage::OnProfileRead,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&AshEventStorage::OnWrite,
                          weak_factory_.GetWeakPtr()));
}

void AshEventStorage::CopyEvents(EventsProto* events_proto) const {
  if (IsPreUserStorageReadable() && pre_user_events()->events_size() > 0) {
    events_proto->mutable_events()->MergeFrom(pre_user_events()->events());
  }
  if (IsProfileReady() && user_events()->events_size() > 0) {
    events_proto->mutable_events()->MergeFrom(user_events()->events());
  }
}

void AshEventStorage::OnWrite(const WriteStatus status) {
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

void AshEventStorage::OnRead(const ReadStatus status) {
  switch (status) {
    case ReadStatus::kOk:
    case ReadStatus::kMissing:
      break;
    case ReadStatus::kReadError:
      LogInternalError(StructuredMetricsError::kEventReadError);
      break;
    case ReadStatus::kParseError:
      LogInternalError(StructuredMetricsError::kEventParseError);
      break;
  }

  OnReady();
}

void AshEventStorage::OnProfileRead(const ReadStatus status) {
  switch (status) {
    case ReadStatus::kOk:
    case ReadStatus::kMissing:
      break;
    case ReadStatus::kReadError:
      LogInternalError(StructuredMetricsError::kEventReadError);
      break;
    case ReadStatus::kParseError:
      LogInternalError(StructuredMetricsError::kEventParseError);
      break;
  }

  OnProfileReady();
}

void AshEventStorage::OnProfileReady() {
  CHECK(user_events_.get());
  is_user_initialized_ = true;

  // Move any events that are current in |pre_user_events_| into the
  // |user_events_|.
  if (pre_user_events() && pre_user_events()->events_size() > 0) {
    RepeatedPtrField<StructuredEventProto>* users_events =
        user_events()->mutable_events();
    RepeatedPtrField<StructuredEventProto>* pre_users_events =
        pre_user_events()->mutable_events();

    // Moving events from |pre_users_events| to |users_events|.
    users_events->Reserve(users_events->size() + pre_users_events->size());

    // Temporary buffer to extract the |pre_users_events| into.
    std::vector<StructuredEventProto*> extracted(pre_users_events->size(),
                                                 nullptr);

    // Extract and add the elements into |users_events|
    pre_users_events->ExtractSubrange(0, pre_users_events->size(),
                                      extracted.data());

    for (auto* element : extracted) {
      users_events->AddAllocated(element);
    }
  }

  // Regardless of if there are any events cleanup the storage.
  if (pre_user_events()) {
    (*pre_user_events_)->Clear();
    pre_user_events_->QueueWrite();
  }

  // The write is fine because it will add to a task that is not tied to the
  // lifetime of |pre_user_events_|.
  pre_user_events_.reset();

  // Dealloc any memory that the vector is occupying as it will not be used
  // anymore.
  std::vector<StructuredEventProto>().swap(pre_storage_events_);
}

bool AshEventStorage::IsProfileReady() const {
  return is_user_initialized_ && user_events_.get();
}

bool AshEventStorage::IsPreUserStorageReadable() const {
  return pre_user_events_ && is_initialized_;
}

PersistentProto<EventsProto>* AshEventStorage::GetStoreToWriteEvent() {
  // If user storage is ready, all events should be stored in user event store
  // regardless of the type.
  if (IsProfileReady()) {
    return user_events_.get();
  }
  // Use the shared storage if user storage is not ready.
  if (IsPreUserStorageReadable()) {
    return pre_user_events_.get();
  }
  return nullptr;
}

}  // namespace metrics::structured
