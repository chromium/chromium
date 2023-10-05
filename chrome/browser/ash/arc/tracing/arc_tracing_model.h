// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_MODEL_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_MODEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ash/arc/tracing/arc_system_model.h"

namespace arc {

class ArcTracingEvent;

// This is a base model that is built from the output of Chrome tracing
// (chrome://tracing). It contains native Chrome test events and system kernel
// events converted to common Chrome test events. Events are kept by thread or
// by group in case of asynchronous events. Events are hierarchical and each
// thread or group is represented by one top-level event.
// There are methods to query the model for particular events.
// |ArcTracingModel| is usually used as a source for more specialized models.
class ArcTracingModel {
 public:
  using TracingEvents = std::vector<std::unique_ptr<ArcTracingEvent>>;
  using TracingEventPtrs = std::vector<const ArcTracingEvent*>;

  ArcTracingModel();

  ArcTracingModel(const ArcTracingModel&) = delete;
  ArcTracingModel& operator=(const ArcTracingModel&) = delete;

  ~ArcTracingModel();

  // Limits events by the requested interval. All events outside of this
  // interval are discarded. |min_timestamp| is inclusive and |max_timestamp| is
  // exclusive.
  void SetMinMaxTime(uint64_t min_timestamp, uint64_t max_timestamp);

  // Builds model from string data in Json format. Returns false if model
  // cannot be built.
  bool Build(const std::string& data);

  // Gets root events.
  TracingEventPtrs GetRoots() const;

  // Selects list of events according to |query|. |query| consists from segments
  // separated by '/' where segment is in format
  // category:name(arg_name=arg_value;..). See ArcTracingEventMatcher for more
  // details. Processing starts from the each root node for thread or group.
  TracingEventPtrs Select(const std::string query) const;
  // Similar to case above but starts from provided event |event|.
  TracingEventPtrs Select(const ArcTracingEvent* event,
                          const std::string query) const;

  // Gets group of asynchronous events for |id|.
  TracingEventPtrs GetGroupEvents(const std::string& id) const;

  // Dumps this model to |stream|.
  void Dump(std::ostream& stream) const;

  ArcSystemModel& system_model() { return system_model_; }
  const ArcSystemModel& system_model() const { return system_model_; }

 private:
  // Processes list of events. Returns true in case all events were processed
  // successfully.
  bool ProcessEvent(base::Value::List* events);

  // Converts sys traces events to the |base::Dictionary| based format used in
  // Chrome.
  bool ConvertSysTraces(const std::string& sys_traces);

  // Adds tracing event to the thread model hierarchy.
  bool AddToThread(std::unique_ptr<ArcTracingEvent> event);

  // Contains events separated by threads. Key is a composition of pid and tid.
  std::map<uint64_t, TracingEvents> per_thread_events_;
  // Contains events, separated by id of the event. Used for asynchronous
  // tracing events.
  std::map<std::string, TracingEvents> group_events_;

  TracingEvents nongroup_events_;

  ArcSystemModel system_model_;

  uint64_t min_timestamp_ = 0;
  uint64_t max_timestamp_ = std::numeric_limits<uint64_t>::max();
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_MODEL_H_
