// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher_tracer.h"

#include "base/json/json_file_value_serializer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace base {

TestLauncherTracer::TestLauncherTracer()
    : trace_start_time_(TimeTicks::Now()) {}

TestLauncherTracer::~TestLauncherTracer() = default;

void TestLauncherTracer::RecordProcessExecution(TimeTicks start_time,
                                                TimeDelta duration) {
  AutoLock lock(lock_);

  Event event;
  event.name = StringPrintf("process #%zu", events_.size());
  event.timestamp = start_time;
  event.duration = duration;
  event.thread_id = PlatformThread::CurrentId();
  events_.push_back(event);
}

bool TestLauncherTracer::Dump(const FilePath& path) {
  AutoLock lock(lock_);

  std::unique_ptr<ListValue> json_events(new ListValue);
  for (const Event& event : events_) {
    std::unique_ptr<DictionaryValue> json_event(new DictionaryValue);
    json_event->SetString("name", event.name);
    json_event->SetString("ph", "X");
    json_event->SetInteger(
        "ts", (event.timestamp - trace_start_time_).InMicroseconds());
    json_event->SetIntKey("dur", event.duration.InMicroseconds());
    json_event->SetIntKey("tid", event.thread_id);

    // Add fake values required by the trace viewer.
    json_event->SetIntKey("pid", 0);

    json_events->Append(std::move(json_event));
  }

  JSONFileValueSerializer serializer(path);
  return serializer.Serialize(*json_events);
}

}  // namespace base
