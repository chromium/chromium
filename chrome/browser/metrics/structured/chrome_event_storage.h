// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_CHROME_EVENT_STORAGE_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_CHROME_EVENT_STORAGE_H_

#include "components/metrics/structured/lib/event_storage.h"
#include "components/metrics/structured/lib/persistent_proto.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

// Storage for Structured Metrics events on Chrome (Windows, Linux, and Mac).
//
// The events are stored in-memory and are lost on crash.
class ChromeEventStorage : public EventStorage<StructuredEventProto> {
 public:
  ChromeEventStorage();

  ChromeEventStorage(const ChromeEventStorage&) = delete;
  ChromeEventStorage& operator=(const ChromeEventStorage&) = delete;

  ~ChromeEventStorage() override;

  // EventStorage:
  void AddEvent(StructuredEventProto event) override;
  ::google::protobuf::RepeatedPtrField<StructuredEventProto> TakeEvents()
      override;
  int RecordedEventsCount() const override;
  void Purge() override;
  void CopyEvents(EventsProto* proto) const override;

 private:
  EventsProto events_;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_CHROME_EVENT_STORAGE_H_
