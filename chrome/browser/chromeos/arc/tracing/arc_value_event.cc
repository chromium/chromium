// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_value_event.h"

namespace arc {

ArcValueEvent::ArcValueEvent(int64_t timestamp, Type type, int value)
    : timestamp(timestamp), type(type), value(value) {}

bool ArcValueEvent::operator==(const ArcValueEvent& other) const {
  return timestamp == other.timestamp && type == other.type &&
         value == other.value;
}

base::ListValue SerializeValueEvents(const ValueEvents& value_events) {
  base::ListValue list;
  for (const auto& event : value_events) {
    base::ListValue event_value;
    event_value.Append(base::Value(static_cast<int>(event.type)));
    event_value.Append(base::Value(static_cast<double>(event.timestamp)));
    event_value.Append(base::Value(event.value));
    list.Append(std::move(event_value));
  }
  return list;
}

bool LoadValueEvents(const base::Value* value, ValueEvents* value_events) {
  if (!value || !value->is_list())
    return false;

  int64_t previous_timestamp = 0;
  for (const auto& entry : value->GetList()) {
    if (!entry.is_list() || entry.GetList().size() != 3)
      return false;
    if (!entry.GetList()[0].is_int())
      return false;
    const ArcValueEvent::Type type =
        static_cast<ArcValueEvent::Type>(entry.GetList()[0].GetInt());
    switch (type) {
      case ArcValueEvent::Type::kMemTotal:
      case ArcValueEvent::Type::kMemUsed:
      case ArcValueEvent::Type::kSwapRead:
      case ArcValueEvent::Type::kSwapWrite:
      case ArcValueEvent::Type::kSwapWait:
      case ArcValueEvent::Type::kGemObjects:
      case ArcValueEvent::Type::kGemSize:
      case ArcValueEvent::Type::kGpuFrequency:
      case ArcValueEvent::Type::kCpuTemperature:
      case ArcValueEvent::Type::kCpuFrequency:
      case ArcValueEvent::Type::kCpuPower:
      case ArcValueEvent::Type::kGpuPower:
      case ArcValueEvent::Type::kMemoryPower:
        break;
      default:
        return false;
    }
    if (!entry.GetList()[1].is_double() && !entry.GetList()[1].is_int())
      return false;
    const int64_t timestamp = entry.GetList()[1].GetDouble();
    if (timestamp < previous_timestamp)
      return false;
    if (!entry.GetList()[2].is_int())
      return false;
    const int value = entry.GetList()[2].GetInt();
    value_events->emplace_back(timestamp, type, value);
    previous_timestamp = timestamp;
  }

  return true;
}

std::ostream& operator<<(std::ostream& os, ArcValueEvent::Type event_type) {
  return os << static_cast<
             typename std::underlying_type<ArcValueEvent::Type>::type>(
             event_type);
}

}  // namespace arc
