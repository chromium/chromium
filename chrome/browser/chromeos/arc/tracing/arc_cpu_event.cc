// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_cpu_event.h"

#include "base/logging.h"

namespace arc {

ArcCpuEvent::ArcCpuEvent(uint64_t timestamp, Type type, uint32_t tid)
    : timestamp(timestamp), type(type), tid(tid) {}

bool ArcCpuEvent::operator==(const ArcCpuEvent& other) const {
  return timestamp == other.timestamp && type == other.type && tid == other.tid;
}

bool AddCpuEvent(CpuEvents* cpu_events,
                 uint64_t timestamp,
                 ArcCpuEvent::Type type,
                 uint32_t tid) {
  // Base validation.
  switch (type) {
    case ArcCpuEvent::Type::kIdleIn:
    case ArcCpuEvent::Type::kIdleOut:
      if (tid) {
        LOG(ERROR) << "Idle should always be bound to the idle process";
        return false;
      }
      break;
    case ArcCpuEvent::Type::kWakeUp:
      if (!tid) {
        LOG(ERROR) << "Cannot wake-up to idle process";
        return false;
      }
      break;
    case ArcCpuEvent::Type::kActive:
      break;
  }

  if (cpu_events->empty()) {
    cpu_events->emplace_back(timestamp, type, tid);
    return true;
  }

  // Verify new event relative to the last event.
  const ArcCpuEvent& last = cpu_events->back();
  if (last.timestamp > timestamp) {
    LOG(ERROR) << "Time sequence is broken for cpu events: " << last.timestamp
               << " vs " << timestamp;
    return false;
  }
  // Check transitions from->to.
  switch (last.type) {
    case ArcCpuEvent::Type::kIdleIn:
      switch (type) {
        case ArcCpuEvent::Type::kIdleOut:
          break;
        case ArcCpuEvent::Type::kWakeUp:
          break;
        default:
          LOG(ERROR) << "Unknown CPU transition: " << last.type << "=>" << type;
          return false;
      }
      break;
    case ArcCpuEvent::Type::kIdleOut:
      switch (type) {
        case ArcCpuEvent::Type::kIdleIn:
          break;
        case ArcCpuEvent::Type::kWakeUp:
          break;
        case ArcCpuEvent::Type::kActive:
          break;
        default:
          LOG(ERROR) << "Unknown CPU transition: " << last.type << "=>" << type;
          return false;
      }
      break;
    case ArcCpuEvent::Type::kWakeUp:
      switch (type) {
        case ArcCpuEvent::Type::kIdleIn:
          break;
        case ArcCpuEvent::Type::kIdleOut:
          break;
        case ArcCpuEvent::Type::kWakeUp:
          break;
        case ArcCpuEvent::Type::kActive:
          break;
        default:
          LOG(ERROR) << "Unknown CPU transition: " << last.type << "=>" << type;
          return false;
      }
      break;
    case ArcCpuEvent::Type::kActive:
      switch (type) {
        case ArcCpuEvent::Type::kIdleIn:
          break;
        case ArcCpuEvent::Type::kWakeUp:
          break;
        case ArcCpuEvent::Type::kActive:
          break;
        default:
          LOG(ERROR) << "Unknown CPU transition: " << last.type << "=>" << type;
          return false;
      }
      break;
  }

  cpu_events->emplace_back(timestamp, type, tid);
  return true;
}

bool AddAllCpuEvent(AllCpuEvents* all_cpu_events,
                    uint32_t cpu_id,
                    uint64_t timestamp,
                    ArcCpuEvent::Type type,
                    uint32_t tid) {
  if (all_cpu_events->size() <= cpu_id)
    all_cpu_events->resize(cpu_id + 1);
  return AddCpuEvent(&(*all_cpu_events)[cpu_id], timestamp, type, tid);
}

base::ListValue SerializeCpuEvents(const CpuEvents& cpu_events) {
  base::ListValue list;
  for (const auto& event : cpu_events) {
    base::ListValue event_value;
    event_value.Append(base::Value(static_cast<int>(event.type)));
    event_value.Append(base::Value(static_cast<double>(event.timestamp)));
    event_value.Append(base::Value(static_cast<int>(event.tid)));
    list.Append(std::move(event_value));
  }
  return list;
}

base::ListValue SerializeAllCpuEvents(const AllCpuEvents& all_cpu_events) {
  base::ListValue list;
  for (const auto& cpu_events : all_cpu_events)
    list.Append(SerializeCpuEvents(cpu_events));
  return list;
}

bool LoadCpuEvents(const base::Value* value, CpuEvents* cpu_events) {
  if (!value || !value->is_list())
    return false;

  uint64_t previous_timestamp = 0;
  for (const auto& entry : value->GetList()) {
    if (!entry.is_list() || entry.GetList().size() != 3)
      return false;
    if (!entry.GetList()[0].is_int())
      return false;
    const ArcCpuEvent::Type type =
        static_cast<ArcCpuEvent::Type>(entry.GetList()[0].GetInt());
    switch (type) {
      case ArcCpuEvent::Type::kIdleIn:
      case ArcCpuEvent::Type::kIdleOut:
      case ArcCpuEvent::Type::kWakeUp:
      case ArcCpuEvent::Type::kActive:
        break;
      default:
        return false;
    }
    if (!entry.GetList()[1].is_double() && !entry.GetList()[1].is_int())
      return false;
    const uint64_t timestamp = entry.GetList()[1].GetDouble();
    if (timestamp < previous_timestamp)
      return false;
    if (!entry.GetList()[2].is_int())
      return false;
    const int tid = entry.GetList()[2].GetInt();
    cpu_events->emplace_back(timestamp, type, tid);
    previous_timestamp = timestamp;
  }

  return true;
}

bool LoadAllCpuEvents(const base::Value* value, AllCpuEvents* all_cpu_events) {
  if (!value || !value->is_list())
    return false;

  for (const auto& entry : value->GetList()) {
    CpuEvents cpu_events;
    if (!LoadCpuEvents(&entry, &cpu_events))
      return false;
    all_cpu_events->emplace_back(std::move(cpu_events));
  }

  return true;
}

std::ostream& operator<<(std::ostream& os, ArcCpuEvent::Type event_type) {
  return os
         << static_cast<typename std::underlying_type<ArcCpuEvent::Type>::type>(
                event_type);
}

}  // namespace arc
