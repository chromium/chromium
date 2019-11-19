// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_EVENT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_EVENT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"

namespace arc {

// |ArcTracingEvent| is a wrapper over |base::DictionaryValue| that is used to
// represent trace event in Chrome. |ArcTracingEvent| is hierarchical and
// can contain children. Setter methods are used to convert system trace events
// that are not dictionary based to the common Chrome format.
class ArcTracingEvent {
 public:
  enum class Position {
    kBefore,   // event is before the compared event
    kInside,   // event is inside the compared event.
    kAfter,    // event is after the compared event.
    kOverlap,  // event overlaps with compared event.
  };

  explicit ArcTracingEvent(base::Value dictionary);
  ~ArcTracingEvent();

  ArcTracingEvent(ArcTracingEvent&&);
  ArcTracingEvent& operator=(ArcTracingEvent&&);

  // Gets process id of the event. Returns 0 if not set.
  int GetPid() const;
  // Sets process id of the event.
  void SetPid(int pid);

  // Gets thread id of the event. Returns 0 if not set.
  int GetTid() const;
  // Sets thread id of the event.
  void SetTid(int tid);

  // Gets id of the group of events. Returns empty string if not set.
  std::string GetId() const;
  // Sets id of the event.
  void SetId(const std::string& id);

  // Gets category of the event. Returns empty string if not set.
  std::string GetCategory() const;
  // Sets category of the event.
  void SetCategory(const std::string& category);

  // Gets name of the event. Returns empty string if not set.
  std::string GetName() const;
  // Sets name of the event.
  void SetName(const std::string& name);

  // Gets phase of the event. Returns 0 if not set.
  char GetPhase() const;
  // Sets phase of the event.
  void SetPhase(char phase);

  // Gets timestamp of the start of the event. Return 0 if not set.
  uint64_t GetTimestamp() const;
  // Sets timestamp of the start of the event.
  void SetTimestamp(uint64_t timestamp);

  // Gets duration of the event.  Return 0 if not set. It is optional for some
  // events.
  uint64_t GetDuration() const;
  // Sets duration of the event.
  void SetDuration(uint64_t duration);

  // Gets timestamp of the end of the event.
  uint64_t GetEndTimestamp() const;

  // Returns base representation of the event as a |base::DictionaryValue|.
  const base::DictionaryValue* GetDictionary() const;

  // Returns set of arguments as a |base::DictionaryValue|.
  const base::DictionaryValue* GetArgs() const;

  // Gets argument as string. Return |default_value| if not found.
  std::string GetArgAsString(const std::string& name,
                             const std::string& default_value) const;

  // Gets argument as integer. Returns |default_value| if not found.
  int GetArgAsInteger(const std::string& name, int default_value) const;

  // Gets argument as double. Returns |default_value| if not found.
  double GetArgAsDouble(const std::string& name, double default_value) const;

  // Classifies the position of another event relative to the current event.
  Position ClassifyPositionOf(const ArcTracingEvent& other) const;

  // Recursively adds child trace event. If child event is not inside the
  // current event than child event is not added and false is returned.
  // Based on building constraints, child element can be appended to the end
  // of the list of child events or to the last child event.
  bool AppendChild(std::unique_ptr<ArcTracingEvent> child);

  // Validates that event contains correct information.
  bool Validate() const;

  // Returns string representation of this event.
  std::string ToString() const;

  // Dumps this event and its children to |stream|. |prefix| is used for
  // formatting.
  void Dump(const std::string& prefix, std::ostream& stream) const;

  const std::vector<std::unique_ptr<ArcTracingEvent>>& children() const {
    return children_;
  }

 private:
  std::vector<std::unique_ptr<ArcTracingEvent>> children_;
  base::Value dictionary_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingEvent);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_EVENT_H_
