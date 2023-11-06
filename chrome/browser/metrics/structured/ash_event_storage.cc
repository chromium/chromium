// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_event_storage.h"

#include "base/functional/callback_forward.h"
#include "base/task/current_thread.h"
#include "components/metrics/structured/histogram_util.h"

namespace metrics::structured {
AshEventStorage::AshEventStorage(base::TimeDelta write_delay)
    : write_delay_(write_delay) {}

AshEventStorage::~AshEventStorage() = default;

// EventStorage:
bool AshEventStorage::IsReady() {
  return events_.get() != nullptr && is_initialized_;
}

void AshEventStorage::OnReady() {
  is_initialized_ = true;

  for (auto& event : pre_storage_events_) {
    AddEvent(std::move(event));
  }

  pre_storage_events_.reserve(0);
}

void AshEventStorage::AddEvent(StructuredEventProto&& event) {
  if (IsReady()) {
    *events()->add_non_uma_events() = event;
  } else {
    pre_storage_events_.emplace_back(event);
  }
}

void AshEventStorage::MoveEvents(ChromeUserMetricsExtension& uma_proto) {
  StructuredDataProto* proto = uma_proto.mutable_structured_data();
  proto->mutable_events()->Swap(events()->mutable_non_uma_events());

  events()->clear_uma_events();
  events()->clear_non_uma_events();
}

void AshEventStorage::Purge() {
  if (IsReady()) {
    events_->Purge();
  }
  // Make sure it is null.
  events_.reset();
}

void AshEventStorage::OnProfileAdded(const base::FilePath& path) {
  DCHECK(base::CurrentUIThread::IsSet());

  if (is_initialized_) {
    return;
  }

  // The directory used to store unsent logs. Relative to the user's cryptohome.
  // This file is created by chromium.
  events_ = std::make_unique<PersistentProto<EventsProto>>(
      path.Append(FILE_PATH_LITERAL("structured_metrics"))
          .Append(FILE_PATH_LITERAL("events")),
      write_delay_,
      base::BindOnce(&AshEventStorage::OnRead, weak_factory_.GetWeakPtr()),
      base::BindRepeating(&AshEventStorage::OnWrite,
                          weak_factory_.GetWeakPtr()));
}

void AshEventStorage::AddBatchEvents(
    const google::protobuf::RepeatedPtrField<StructuredEventProto>& events) {
  AshEventStorage::events()->mutable_non_uma_events()->MergeFrom(events);
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

}  // namespace metrics::structured
