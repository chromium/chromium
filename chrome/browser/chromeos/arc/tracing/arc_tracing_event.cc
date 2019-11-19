// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/common/trace_event_common.h"

namespace arc {

namespace {

constexpr char kKeyArguments[] = "args";
constexpr char kKeyDuration[] = "dur";
constexpr char kKeyCategory[] = "cat";
constexpr char kKeyId[] = "id";
constexpr char kKeyName[] = "name";
constexpr char kKeyPid[] = "pid";
constexpr char kKeyPhase[] = "ph";
constexpr char kKeyTid[] = "tid";
constexpr char kKeyTimestamp[] = "ts";

int GetIntegerFromDictionary(const base::DictionaryValue* dictionary,
                             const std::string& name,
                             int default_value) {
  if (!dictionary)
    return default_value;
  const base::Value* value =
      dictionary->FindKeyOfType(name, base::Value::Type::INTEGER);
  return value ? value->GetInt() : default_value;
}

double GetDoubleFromDictionary(const base::DictionaryValue* dictionary,
                               const std::string& name,
                               double default_value) {
  if (!dictionary)
    return default_value;
  const base::Value* value =
      dictionary->FindKeyOfType(name, base::Value::Type::DOUBLE);
  if (value)
    return value->GetDouble();
  value = dictionary->FindKeyOfType(name, base::Value::Type::INTEGER);
  if (value)
    return value->GetInt();
  return default_value;
}

std::string GetStringFromDictionary(const base::DictionaryValue* dictionary,
                                    const std::string& name,
                                    const std::string& default_value) {
  if (!dictionary)
    return default_value;
  const base::Value* value =
      dictionary->FindKeyOfType(name, base::Value::Type::STRING);
  return value ? value->GetString() : default_value;
}

}  // namespace

ArcTracingEvent::ArcTracingEvent(base::Value dictionary)
    : dictionary_(std::move(dictionary)) {}

ArcTracingEvent::~ArcTracingEvent() = default;

ArcTracingEvent::ArcTracingEvent(ArcTracingEvent&&) = default;

ArcTracingEvent& ArcTracingEvent::operator=(ArcTracingEvent&&) = default;

int ArcTracingEvent::GetPid() const {
  return GetIntegerFromDictionary(GetDictionary(), kKeyPid,
                                  0 /* default_value */);
}

void ArcTracingEvent::SetPid(int pid) {
  dictionary_.SetKey(kKeyPid, base::Value(pid));
}

int ArcTracingEvent::GetTid() const {
  return GetIntegerFromDictionary(GetDictionary(), kKeyTid,
                                  0 /* default_value */);
}

void ArcTracingEvent::SetTid(int tid) {
  dictionary_.SetKey(kKeyTid, base::Value(tid));
}

std::string ArcTracingEvent::GetId() const {
  return GetStringFromDictionary(GetDictionary(), kKeyId,
                                 std::string() /* default_value */);
}

void ArcTracingEvent::SetId(const std::string& id) {
  dictionary_.SetKey(kKeyId, base::Value(id));
}

std::string ArcTracingEvent::GetCategory() const {
  return GetStringFromDictionary(GetDictionary(), kKeyCategory,
                                 std::string() /* default_value */);
}

void ArcTracingEvent::SetCategory(const std::string& category) {
  dictionary_.SetKey(kKeyCategory, base::Value(category));
}

std::string ArcTracingEvent::GetName() const {
  return GetStringFromDictionary(GetDictionary(), kKeyName,
                                 std::string() /* default_value */);
}

void ArcTracingEvent::SetName(const std::string& name) {
  dictionary_.SetKey(kKeyName, base::Value(name));
}

char ArcTracingEvent::GetPhase() const {
  const std::string ph = GetStringFromDictionary(
      GetDictionary(), kKeyPhase, std::string() /* default_value */);
  return ph.length() == 1 ? ph[0] : '\0';
}

void ArcTracingEvent::SetPhase(char phase) {
  dictionary_.SetKey(kKeyPhase, base::Value(std::string() + phase));
}

uint64_t ArcTracingEvent::GetTimestamp() const {
  return GetDoubleFromDictionary(GetDictionary(), kKeyTimestamp,
                                 0.0 /* default_value */);
}

void ArcTracingEvent::SetTimestamp(uint64_t timestamp) {
  dictionary_.SetKey(kKeyTimestamp,
                     base::Value(static_cast<double>(timestamp)));
}

uint64_t ArcTracingEvent::GetDuration() const {
  return GetDoubleFromDictionary(GetDictionary(), kKeyDuration,
                                 0.0 /* default_value */);
}

void ArcTracingEvent::SetDuration(uint64_t duration) {
  dictionary_.SetKey(kKeyDuration, base::Value(static_cast<double>(duration)));
}

uint64_t ArcTracingEvent::GetEndTimestamp() const {
  return GetTimestamp() + GetDuration();
}

const base::DictionaryValue* ArcTracingEvent::GetDictionary() const {
  const base::DictionaryValue* dictionary = nullptr;
  dictionary_.GetAsDictionary(&dictionary);
  return dictionary;
}

const base::DictionaryValue* ArcTracingEvent::GetArgs() const {
  const base::Value* value =
      dictionary_.FindKeyOfType(kKeyArguments, base::Value::Type::DICTIONARY);
  if (!value)
    return nullptr;
  const base::DictionaryValue* args = nullptr;
  value->GetAsDictionary(&args);
  return args;
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
  if (this_start <= other_start && this_end >= other_end)
    return Position::kInside;
  if (this_end <= other_start)
    return Position::kAfter;
  if (other_end <= this_start)
    return Position::kBefore;
  return Position::kOverlap;
}

bool ArcTracingEvent::AppendChild(std::unique_ptr<ArcTracingEvent> child) {
  if (ClassifyPositionOf(*child) != Position::kInside)
    return false;

  if (children_.empty() ||
      children_.back()->ClassifyPositionOf(*child) == Position::kAfter) {
    children_.push_back(std::move(child));
    return true;
  }
  return children_.back()->AppendChild(std::move(child));
}

bool ArcTracingEvent::Validate() const {
  if (!GetPid() || GetCategory().empty() || GetName().empty())
    return false;

  switch (GetPhase()) {
    case TRACE_EVENT_PHASE_COMPLETE:
    case TRACE_EVENT_PHASE_COUNTER:
      if (!GetTid())
        return false;
      break;
    case TRACE_EVENT_PHASE_METADATA:
      break;
    case TRACE_EVENT_PHASE_ASYNC_BEGIN:
    case TRACE_EVENT_PHASE_ASYNC_END:
    case TRACE_EVENT_PHASE_ASYNC_STEP_INTO:
      if (!GetTid() || GetId().empty())
        return false;
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
  const base::DictionaryValue* args = GetArgs();
  if (args) {
    bool first_arg = true;
    for (const auto& arg : *args) {
      if (first_arg) {
        result += "|";
        first_arg = false;
      } else {
        result += ",";
      }
      int int_value;
      double double_value;
      std::string string_value;
      if (arg.second->GetAsString(&string_value)) {
        result += base::StringPrintf("%s=%s", arg.first.c_str(),
                                     string_value.c_str());
      } else if (arg.second->GetAsInteger(&int_value)) {
        result += base::StringPrintf("%s=%i", arg.first.c_str(), int_value);
      } else if (arg.second->GetAsDouble(&double_value)) {
        result += base::StringPrintf("%s=%f", arg.first.c_str(), double_value);
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
  for (const auto& event : children_)
    event->Dump(prefix + "  ", stream);
}

}  // namespace arc
