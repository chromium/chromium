// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_event_storage.h"

#include "base/functional/callback_forward.h"
#include "base/task/current_thread.h"
#include "components/metrics/structured/histogram_util.h"

namespace metrics::structured {

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

void AshEventStorage::AddEvent(StructuredEventProto&& event) {
  PersistentProto<EventsProto>* event_store_to_write =
      GetStoreToWriteEvent(event);

  if (!event_store_to_write) {
    pre_storage_events_.emplace_back(event);
    return;
  }

  *event_store_to_write->get()->add_non_uma_events() = event;
  event_store_to_write->StartWrite();
}

void AshEventStorage::MoveEvents(ChromeUserMetricsExtension& uma_proto) {
  StructuredDataProto* proto = uma_proto.mutable_structured_data();

  if (IsPreUserStorageReadable() &&
      pre_user_events()->non_uma_events_size() > 0) {
    proto->mutable_events()->MergeFrom(pre_user_events()->non_uma_events());
    pre_user_events()->clear_non_uma_events();
    pre_user_events_->StartWrite();
  }
  if (IsProfileReady() && user_events()->non_uma_events_size() > 0) {
    proto->mutable_events()->MergeFrom(user_events()->non_uma_events());
    user_events()->clear_non_uma_events();
    user_events_->StartWrite();
  }

  // TODO(b/312292811): Cleanup |pre_user_events_| after the first upload as it
  // is not needed. This cannot be done currently because the dtor will trigger
  // a blocking call on a non-blocking thread.
}

int AshEventStorage::RecordedEventsCount() const {
  int total_event_count = 0;
  if (IsPreUserStorageReadable()) {
    total_event_count += pre_user_events_->get()->non_uma_events_size();
  }
  if (is_user_initialized_) {
    total_event_count += user_events_->get()->non_uma_events_size();
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

void AshEventStorage::OnProfileAdded(const base::FilePath& path) {
  DCHECK(base::CurrentUIThread::IsSet());

  if (is_user_initialized_) {
    return;
  }

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

void AshEventStorage::AddBatchEvents(
    const google::protobuf::RepeatedPtrField<StructuredEventProto>& events) {
  for (const auto& event : events) {
    PersistentProto<EventsProto>* event_store = GetStoreToWriteEvent(event);

    if (event_store) {
      *event_store->get()->add_non_uma_events() = event;
      event_store->StartWrite();
      continue;
    }

    if (!is_initialized_) {
      pre_storage_events_.emplace_back(event);
    }
  }
}

void AshEventStorage::GetEvents(EventsProto* proto) {
  if (IsPreUserStorageReadable() &&
      pre_user_events()->non_uma_events_size() > 0) {
    proto->mutable_non_uma_events()->MergeFrom(
        pre_user_events()->non_uma_events());
  }
  if (IsProfileReady() && user_events()->non_uma_events_size() > 0) {
    proto->mutable_non_uma_events()->MergeFrom(user_events()->non_uma_events());
  }
}

void AshEventStorage::OnWrite(const WriteStatus status) {
  DCHECK(base::CurrentUIThread::IsSet());

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
  DCHECK(base::CurrentUIThread::IsSet());

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
  DCHECK(base::CurrentUIThread::IsSet());

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

PersistentProto<EventsProto>* AshEventStorage::GetStoreToWriteEvent(
    const StructuredEventProto& event) {
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
