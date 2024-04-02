// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_tracing_event.h"

#include <inttypes.h>

#include <ostream>

#include "base/strings/stringprintf.h"
#include "base/trace_event/common/trace_event_common.h"

namespace arc {

namespace {

constexpr char kKeyArguments[] = "args";
constexpr char kKeyDuration[] = "dur";
constexpr char kKeyCategory[] = "cat";
constexpr char kKeyId[] = "id";
constexpr char kKeyId2[] = "id2";
constexpr char kKeyLocal[] = "local";
constexpr char kKeyName[] = "name";
constexpr char kKeyPid[] = "pid";
constexpr char kKeyPhase[] = "ph";
constexpr char kKeyTid[] = "tid";
constexpr char kKeyTimestamp[] = "ts";

int GetIntegerFromDictionary(const base::Value::Dict* dictionary,
                             const std::string& name,
                             int default_value) {
  if (!dictionary) {
    return default_value;
  }
  return dictionary->FindInt(name).value_or(default_value);
}

double GetDoubleFromDictionary(const base::Value::Dict* dictionary,
                               const std::string& name,
                               double default_value) {
  if (!dictionary) {
    return default_value;
  }
  std::optional<double> double_value = dictionary->FindDouble(name);
  if (double_value) {
    return *double_value;
  }
  std::optional<int> int_value = dictionary->FindInt(name);
  if (int_value) {
    return *int_value;
  }
  return default_value;
}

std::string GetStringFromDictionary(const base::Value::Dict* dictionary,
                                    const std::string& name,
                                    const std::string& default_value) {
  if (!dictionary) {
    return default_value;
  }
  const std::string* value = dictionary->FindString(name);
  return value ? *value : default_value;
}

}  // namespace

ArcTracingEvent::ArcTracingEvent(base::Value::Dict dictionary)
    : dictionary_(std::move(dictionary)) {}

ArcTracingEvent::~ArcTracingEvent() = default;

ArcTracingEvent::ArcTracingEvent(ArcTracingEvent&&) = default;

ArcTracingEvent& ArcTracingEvent::operator=(ArcTracingEvent&&) = default;

int ArcTracingEvent::GetPid() const {
  return GetIntegerFromDictionary(GetDictionary(), kKeyPid,
                                  0 /* default_value */);
}

void ArcTracingEvent::SetPid(int pid) {
  dictionary_.Set(kKeyPid, pid);
}

int ArcTracingEvent::GetTid() const {
  return GetIntegerFromDictionary(GetDictionary(), kKeyTid,
                                  0 /* default_value */);
}

void ArcTracingEvent::SetTid(int tid) {
  dictionary_.Set(kKeyTid, tid);
}

std::string ArcTracingEvent::GetId() const {
  const base::Value::Dict* dictionary = GetDictionary();
  const std::string* id_value = dictionary->FindString(kKeyId);
  if (id_value) {
    return *id_value;
  }

  const base::Value::Dict* id2_value = dictionary->FindDict(kKeyId2);
  if (id2_value) {
    return GetStringFromDictionary(id2_value, kKeyLocal,
                                   std::string() /* default_value */);
  }
  return std::string();
}

void ArcTracingEvent::SetId(const std::string& id) {
  dictionary_.Set(kKeyId, id);
}

std::string ArcTracingEvent::GetCategory() const {
  return GetStringFromDictionary(GetDictionary(), kKeyCategory,
                                 std::string() /* default_value */);
}

void ArcTracingEvent::SetCategory(const std::string& category) {
  dictionary_.Set(kKeyCategory, category);
}

std::string ArcTracingEvent::GetName() const {
  return GetStringFromDictionary(GetDictionary(), kKeyName,
                                 std::string() /* default_value */);
}

void ArcTracingEvent::SetName(const std::string& name) {
  dictionary_.Set(kKeyName, name);
}

char ArcTracingEvent::GetPhase() const {
  const std::string ph = GetStringFromDictionary(
      GetDictionary(), kKeyPhase, std::string() /* default_value */);
  return ph.length() == 1 ? ph[0] : '\0';
}

void ArcTracingEvent::SetPhase(char phase) {
  dictionary_.Set(kKeyPhase, std::string() + phase);
}

uint64_t ArcTracingEvent::GetTimestamp() const {
  return GetDoubleFromDictionary(GetDictionary(), kKeyTimestamp,
                                 0.0 /* default_value */);
}

void ArcTracingEvent::SetTimestamp(uint64_t timestamp) {
  dictionary_.Set(kKeyTimestamp, static_cast<double>(timestamp));
}

uint64_t ArcTracingEvent::GetDuration() const {
  return GetDoubleFromDictionary(GetDictionary(), kKeyDuration,
                                 0.0 /* default_value */);
}

void ArcTracingEvent::SetDuration(uint64_t duration) {
  dictionary_.Set(kKeyDuration, static_cast<double>(duration));
}

uint64_t ArcTracingEvent::GetEndTimestamp() const {
  return GetTimestamp() + GetDuration();
}

const base::Value::Dict* ArcTracingEvent::GetDictionary() const {
  return &dictionary_;
}

const base::Value::Dict* ArcTracingEvent::GetArgs() const {
  return dictionary_.FindDict(kKeyArguments);
}

std::string ArcTracingEvent::GetArgAsString(
    const std::string& name,
    const std::string& default_value) const {
  return GetStringFromDictionary(GetArgs(), name, default_value);
}

int ArcTracingEvent::GetArgAsInteger(const std::string& name,
                                     int default_value) const {
  return GetIntegerFromDictionary(GetArgs(), name, default_value);
}

double ArcTracingEvent::GetArgAsDouble(const std::string& name,
                                       double default_value) const {
  return GetDoubleFromDictionary(GetArgs(), name, default_value);
}

ArcTracingEvent::Position ArcTracingEvent::ClassifyPositionOf(
    const ArcTracingEvent& other) const {
  const int64_t this_start = GetTimestamp();
  const int64_t this_end = this_start + GetDuration();
  const int64_t other_start = other.GetTimestamp();
  const int64_t other_end = other_start + other.GetDuration();
  if (this_start <= other_start && this_end >= other_end) {
    return Position::kInside;
  }
  if (this_end <= other_start) {
    return Position::kAfter;
  }
  if (other_end <= this_start) {
    return Position::kBefore;
  }
  return Position::kOverlap;
}

bool ArcTracingEvent::AppendChild(std::unique_ptr<ArcTracingEvent> child) {
  if (ClassifyPositionOf(*child) != Position::kInside) {
    return false;
  }

  if (children_.empty() ||
      children_.back()->ClassifyPositionOf(*child) == Position::kAfter) {
    children_.push_back(std::move(child));
    return true;
  }
  return children_.back()->AppendChild(std::move(child));
}

bool ArcTracingEvent::Validate() const {
  const std::string name = GetName();
  if (GetCategory().empty() || name.empty()) {
    return false;
  }
  if (name == "ActiveProcesses") {
    // Does not have pid or tid, so will not pass below checks.
    return true;
  }
  if (!GetPid()) {
    return false;
  }

  switch (GetPhase()) {
    case TRACE_EVENT_PHASE_COMPLETE:
    case TRACE_EVENT_PHASE_COUNTER:
    case TRACE_EVENT_PHASE_INSTANT:
      if (!GetTid()) {
        return false;
      }
      break;
    case TRACE_EVENT_PHASE_METADATA:
      break;
    case TRACE_EVENT_PHASE_ASYNC_BEGIN:
    case TRACE_EVENT_PHASE_ASYNC_END:
    case TRACE_EVENT_PHASE_ASYNC_STEP_INTO:
      if (!GetTid() || GetId().empty()) {
        return false;
      }
      break;
    default:
      return false;
  }
  return true;
}

std::string ArcTracingEvent::ToString() const {
  std::string result = base::StringPrintf(
      "%d|%d|%" PRId64 "|%" PRId64 "|%c|%s|%s|%s", GetPid(), GetTid(),
      GetTimestamp(), GetDuration(), GetPhase(), GetCategory().c_str(),
      GetName().c_str(), GetId().c_str());
  const base::Value::Dict* args = GetArgs();
  if (args) {
    bool first_arg = true;
    for (const auto arg : *args) {
      if (first_arg) {
        result += "|";
        first_arg = false;
      } else {
        result += ",";
      }
      if (arg.second.is_string()) {
        result += base::StringPrintf("%s=%s", arg.first.c_str(),
                                     arg.second.GetString().c_str());
      } else if (arg.second.is_int()) {
        result +=
            base::StringPrintf("%s=%i", arg.first.c_str(), arg.second.GetInt());
      } else if (arg.second.is_double()) {
        result += base::StringPrintf("%s=%f", arg.first.c_str(),
                                     arg.second.GetDouble());
      } else {
        result += base::StringPrintf("%s=?", arg.first.c_str());
      }
    }
  }
  return result;
}

void ArcTracingEvent::Dump(const std::string& prefix,
                           std::ostream& stream) const {
  stream << prefix << ToString() << "\n";
  for (const auto& event : children_) {
    event->Dump(prefix + "  ", stream);
  }
}

}  // namespace arc
