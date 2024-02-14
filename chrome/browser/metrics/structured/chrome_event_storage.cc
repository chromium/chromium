// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/chrome_event_storage.h"
#include "components/metrics/structured/histogram_util.h"

namespace metrics::structured {

using ::google::protobuf::RepeatedPtrField;

ChromeEventStorage::ChromeEventStorage() = default;

ChromeEventStorage::~ChromeEventStorage() = default;

void ChromeEventStorage::AddEvent(StructuredEventProto event) {
  *events_.add_non_uma_events() = std::move(event);
}

RepeatedPtrField<StructuredEventProto> ChromeEventStorage::TakeEvents() {
  return std::move(*events_.mutable_non_uma_events());
}

int ChromeEventStorage::RecordedEventsCount() const {
  return events_.non_uma_events_size();
}

void ChromeEventStorage::Purge() {
  events_.clear_non_uma_events();
}

void ChromeEventStorage::CopyEvents(EventsProto* proto) const {
  proto->mutable_non_uma_events()->MergeFrom(events_.non_uma_events());
}

}  // namespace metrics::structured
