// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"

#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event_matcher.h"

namespace arc {

namespace {

constexpr char kAndroidCategory[] = "android";
constexpr char kTracingMarkWrite[] = ": tracing_mark_write: ";
constexpr int kTracingMarkWriteLength = sizeof(kTracingMarkWrite) - 1;
constexpr char kCpuIdle[] = ": cpu_idle: ";
constexpr int kCpuIdleLength = sizeof(kCpuIdle) - 1;
constexpr char kIntelGpuFreqChange[] = ": intel_gpu_freq_change: ";
constexpr int kIntelGpuFreqChangeLength = sizeof(kIntelGpuFreqChange) - 1;
constexpr char kSchedWakeUp[] = ": sched_wakeup: ";
constexpr int kSchedWakeUpLength = sizeof(kSchedWakeUp) - 1;
constexpr char kSchedSwitch[] = ": sched_switch: ";
constexpr int kSchedSwitchLength = sizeof(kSchedSwitch) - 1;
constexpr char kTraceEventClockSync[] = "trace_event_clock_sync: ";
constexpr int kTraceEventClockSyncLength = sizeof(kTraceEventClockSync) - 1;

// Helper function that converts a portion of string to uint32_t value. |pos|
// specifies the position in string to parse. |end_char| specifies expected
// character at the end. \0 means parse to the end. Returns the position of
// the next character after parsed digits or std::string::npos in case parsing
// failed. This is performance oriented and main idea is to avoid extra memory
// allocations due to sub-string extractions.
size_t ParseUint32(const std::string& str,
                   size_t pos,
                   char end_char,
                   uint32_t* res) {
  *res = 0;
  const size_t len = str.length();
  while (true) {
    const char& c = str[pos];
    if (c != ' ')
      break;
    if (++pos == len)
      return std::string::npos;
  }

  while (true) {
    const char& c = str[pos];
    if (c < '0' || c > '9')
      return std::string::npos;
    const uint32_t prev = *res;
    *res = *res * 10 + c - '0';
    if (prev != (*res - c + '0') / 10)
      return std::string::npos;  // overflow
    if (++pos == len || str[pos] == end_char)
      break;
  }

  return pos;
}

std::vector<std::unique_ptr<ArcTracingEventMatcher>> BuildSelector(
    const std::string& query) {
  std::vector<std::unique_ptr<ArcTracingEventMatcher>> result;
  for (const std::string& segment : base::SplitString(
           query, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    result.emplace_back(std::make_unique<ArcTracingEventMatcher>(segment));
  }
  return result;
}

void SelectRecursively(
    size_t level,
    const ArcTracingEvent* event,
    const std::vector<std::unique_ptr<ArcTracingEventMatcher>>& selector,
    ArcTracingModel::TracingEventPtrs* collector) {
  if (level >= selector.size())
    return;
  if (!selector[level]->Match(*event))
    return;
  if (level == selector.size() - 1) {
    // Last segment
    collector->push_back(event);
  } else {
    for (const auto& child : event->children())
      SelectRecursively(level + 1, child.get(), selector, collector);
  }
}

struct GraphicsEventsContext {
  // To keep in correct order of creation. This converts pair of 'B' and 'E'
  // events to the completed event, 'X'.
  ArcTracingModel::TracingEvents converted_events;
  std::map<uint32_t, std::vector<ArcTracingEvent*>>
      per_thread_pending_events_stack;

  std::map<std::pair<char, std::string>, std::unique_ptr<ArcTracingEvent>>
      pending_asynchronous_events;
};

bool HandleGraphicsEvent(GraphicsEventsContext* context,
                         uint64_t timestamp,
                         uint32_t tid,
                         const std::string& line,
                         size_t event_position) {
  if (event_position + kTraceEventClockSyncLength < line.length() &&
      !strncmp(&line[event_position], kTraceEventClockSync,
               kTraceEventClockSyncLength)) {
    // Ignore this service message.
    return true;
  }

  if (line[event_position + 1] != '|') {
    LOG(ERROR) << "Cannot recognize trace marker event: " << line;
    return false;
  }

  const char phase = line[event_position];

  uint32_t pid;
  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
    case TRACE_EVENT_PHASE_COUNTER: {
      const size_t name_pos = ParseUint32(line, event_position + 2, '|', &pid);
      if (name_pos == std::string::npos) {
        LOG(ERROR) << "Cannot parse pid of trace event: " << line;
        return false;
      }
      const std::string name = line.substr(name_pos + 1);
      std::unique_ptr<ArcTracingEvent> event =
          std::make_unique<ArcTracingEvent>(base::DictionaryValue());
      event->SetPid(pid);
      event->SetTid(tid);
      event->SetTimestamp(timestamp);
      event->SetCategory(kAndroidCategory);
      event->SetName(name);
      if (phase == TRACE_EVENT_PHASE_BEGIN)
        context->per_thread_pending_events_stack[tid].push_back(event.get());
      else
        event->SetPhase(TRACE_EVENT_PHASE_COUNTER);
      context->converted_events.push_back(std::move(event));
    } break;
    case TRACE_EVENT_PHASE_END: {
      // Beginning event may not exist.
      if (context->per_thread_pending_events_stack[tid].empty())
        return true;
      if (ParseUint32(line, event_position + 2, '\0', &pid) ==
          std::string::npos) {
        LOG(ERROR) << "Cannot parse pid of trace event: " << line;
        return false;
      }
      ArcTracingEvent* completed_event =
          context->per_thread_pending_events_stack[tid].back();
      context->per_thread_pending_events_stack[tid].pop_back();
      completed_event->SetPhase(TRACE_EVENT_PHASE_COMPLETE);
      completed_event->SetDuration(timestamp - completed_event->GetTimestamp());
    } break;
    case TRACE_EVENT_PHASE_ASYNC_BEGIN:
    case TRACE_EVENT_PHASE_ASYNC_END: {
      const size_t name_pos = ParseUint32(line, event_position + 2, '|', &pid);
      if (name_pos == std::string::npos) {
        LOG(ERROR) << "Cannot parse pid of trace event: " << line;
        return false;
      }
      const size_t id_pos = line.find('|', name_pos + 2);
      if (id_pos == std::string::npos) {
        LOG(ERROR) << "Cannot parse name|id of trace event: " << line;
        return false;
      }
      const std::string name = line.substr(name_pos + 1, id_pos - name_pos - 1);
      const std::string id = line.substr(id_pos + 1);
      std::unique_ptr<ArcTracingEvent> event =
          std::make_unique<ArcTracingEvent>(base::DictionaryValue());
      event->SetPhase(phase);
      event->SetPid(pid);
      event->SetTid(tid);
      event->SetTimestamp(timestamp);
      event->SetCategory(kAndroidCategory);
      event->SetName(name);
      // Id here is weak and theoretically can be replicated in another
      // processes or for different event names.
      const std::string full_id = line.substr(event_position + 2);
      event->SetId(id);
      if (context->pending_asynchronous_events.find({phase, full_id}) !=
          context->pending_asynchronous_events.end()) {
        LOG(ERROR) << "Found duplicated asynchronous event " << line;
        // That could be the real case from Android framework, for example
        // animator:opacity trace. Ignore these duplicate events.
        return true;
      }
      context->pending_asynchronous_events[{phase, full_id}] = std::move(event);
    } break;
    default:
      LOG(ERROR) << "Unsupported type of trace event: " << line;
      return false;
  }
  return true;
}

bool HandleCpuIdle(AllCpuEvents* all_cpu_events,
                   uint64_t timestamp,
                   uint32_t cpu_id,
                   uint32_t tid,
                   const std::string& line,
                   size_t event_position) {
  if (tid) {
    LOG(ERROR) << "cpu_idle belongs to non-idle thread: " << line;
    return false;
  }
  uint32_t state;
  uint32_t cpu_id_from_event;
  if (sscanf(&line[event_position], "state=%" SCNu32 " cpu_id=%" SCNu32, &state,
             &cpu_id_from_event) != 2 ||
      cpu_id != cpu_id_from_event) {
    LOG(ERROR) << "Failed to parse cpu_idle event: " << line;
    return false;
  }

  return AddAllCpuEvent(all_cpu_events, cpu_id, timestamp,
                        state == 0xffffffff ? ArcCpuEvent::Type::kIdleOut
                                            : ArcCpuEvent::Type::kIdleIn,
                        0 /* tid */);
}

bool HandleSchedWakeUp(AllCpuEvents* all_cpu_events,
                       uint64_t timestamp,
                       uint32_t cpu_id,
                       uint32_t tid,
                       const std::string& line,
                       size_t event_position) {
  const char* data = strstr(&line[event_position], " pid=");
  uint32_t target_tid;
  uint32_t target_priority;
  uint32_t success;
  uint32_t target_cpu_id;
  if (!data) {
    LOG(ERROR) << "Failed to parse sched_wakeup event: " << line;
    return false;
  }

  bool parsed = false;

  // Try different kernel formats. In case one does not match, don't attempt to
  // use it in the future.
  {
    static bool use_this = true;
    if (!parsed && use_this) {
      parsed =
          sscanf(data, " pid=%" SCNu32 " prio=%" SCNu32 " target_cpu=%" SCNu32,
                 &target_tid, &target_priority, &target_cpu_id) == 3;
      use_this = parsed;
    }
  }

  {
    static bool use_this = true;
    if (!parsed && use_this) {
      parsed =
          sscanf(data,
                 " pid=%" SCNu32 " prio=%" SCNu32 " success=%" SCNu32
                 " target_cpu=%" SCNu32,
                 &target_tid, &target_priority, &success, &target_cpu_id) == 4;
      use_this = parsed;
    }
  }

  if (!parsed) {
    LOG(ERROR) << "Failed to parse sched_wakeup event: " << line;
    return false;
  }

  if (!target_tid) {
    LOG(ERROR) << "Cannot wake-up idle thread: " << line;
    return false;
  }

  return AddAllCpuEvent(all_cpu_events, target_cpu_id, timestamp,
                        ArcCpuEvent::Type::kWakeUp, target_tid);
}

bool HandleSchedSwitch(AllCpuEvents* all_cpu_events,
                       uint64_t timestamp,
                       uint32_t cpu_id,
                       uint32_t tid,
                       const std::string& line,
                       size_t event_position) {
  const char* data = strstr(&line[event_position], " next_pid=");
  uint32_t next_tid;
  uint32_t next_priority;
  if (!data || sscanf(data, " next_pid=%d next_prio=%d", &next_tid,
                      &next_priority) != 2) {
    LOG(ERROR) << "Failed to parse sched_switch event: " << line;
    return false;
  }

  return AddAllCpuEvent(all_cpu_events, cpu_id, timestamp,
                        ArcCpuEvent::Type::kActive, next_tid);
}

bool HandleGpuFreq(ValueEvents* value_events,
                   uint64_t timestamp,
                   const std::string& line,
                   size_t event_position) {
  int new_freq = -1;
  if (sscanf(&line[event_position], "new_freq=%d", &new_freq) != 1) {
    LOG(ERROR) << "Failed to parse GPU freq event: " << line;
    return false;
  }

  value_events->emplace_back(timestamp, ArcValueEvent::Type::kGpuFrequency,
                             new_freq);
  return true;
}

bool SortByTimestampPred(const std::unique_ptr<ArcTracingEvent>& lhs,
                         const std::unique_ptr<ArcTracingEvent>& rhs) {
  const uint64_t lhs_timestamp = lhs->GetTimestamp();
  const uint64_t rhs_timestamp = rhs->GetTimestamp();
  if (lhs_timestamp != rhs_timestamp)
    return lhs_timestamp < rhs_timestamp;
  return lhs->GetDuration() > rhs->GetDuration();
}

}  // namespace

ArcTracingModel::ArcTracingModel() = default;

ArcTracingModel::~ArcTracingModel() = default;

void ArcTracingModel::SetMinMaxTime(uint64_t min_timestamp,
                                    uint64_t max_timestamp) {
  DCHECK(min_timestamp < max_timestamp);
  min_timestamp_ = min_timestamp;
  max_timestamp_ = max_timestamp;
}

bool ArcTracingModel::Build(const std::string& data) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(data);
  if (!value) {
    LOG(ERROR) << "Cannot parse trace data";
    return false;
  }

  base::DictionaryValue* dictionary;
  if (!value->GetAsDictionary(&dictionary)) {
    LOG(ERROR) << "Trace data is not dictionary";
    return false;
  }

  std::string sys_traces;
  if (dictionary->GetString("systemTraceEvents", &sys_traces)) {
    if (!ConvertSysTraces(sys_traces)) {
      LOG(ERROR) << "Failed to convert systrace data";
      return false;
    }
  }

  base::ListValue* events;
  if (!dictionary->GetList("traceEvents", &events)) {
    LOG(ERROR) << "No trace events";
    return false;
  }

  if (!ProcessEvent(events)) {
    LOG(ERROR) << "Failed to process events";
    return false;
  }

  for (auto& group_events : group_events_) {
    std::sort(group_events.second.begin(), group_events.second.end(),
              SortByTimestampPred);
  }

  return true;
}

ArcTracingModel::TracingEventPtrs ArcTracingModel::GetRoots() const {
  ArcTracingModel::TracingEventPtrs result;
  for (auto& gr : group_events_) {
    for (const auto& event : gr.second)
      result.emplace_back(event.get());
  }

  for (const auto& gr : per_thread_events_) {
    for (const auto& event : gr.second)
      result.emplace_back(event.get());
  }
  return result;
}

ArcTracingModel::TracingEventPtrs ArcTracingModel::Select(
    const std::string query) const {
  ArcTracingModel::TracingEventPtrs collector;
  const std::vector<std::unique_ptr<ArcTracingEventMatcher>> selector =
      BuildSelector(query);
  for (const ArcTracingEvent* root : GetRoots())
    SelectRecursively(0, root, selector, &collector);

  return collector;
}

ArcTracingModel::TracingEventPtrs ArcTracingModel::Select(
    const ArcTracingEvent* event,
    const std::string query) const {
  ArcTracingModel::TracingEventPtrs collector;
  for (const auto& child : event->children())
    SelectRecursively(0, child.get(), BuildSelector(query), &collector);
  return collector;
}

ArcTracingModel::TracingEventPtrs ArcTracingModel::GetGroupEvents(
    const std::string& id) const {
  TracingEventPtrs result;
  const auto& it = group_events_.find(id);
  if (it == group_events_.end())
    return result;
  for (const auto& group_event : it->second)
    result.emplace_back(group_event.get());
  return result;
}

bool ArcTracingModel::ProcessEvent(base::ListValue* events) {
  std::vector<std::unique_ptr<ArcTracingEvent>> parsed_events;
  for (auto& it : events->GetList()) {
    base::Value event_data = std::move(it);
    if (!event_data.is_dict()) {
      LOG(ERROR) << "Event is not a dictionary";
      return false;
    }

    std::unique_ptr<ArcTracingEvent> event =
        std::make_unique<ArcTracingEvent>(std::move(event_data));
    const uint64_t timestamp = event->GetTimestamp();
    if (timestamp < min_timestamp_ || timestamp >= max_timestamp_)
      continue;

    switch (event->GetPhase()) {
      case TRACE_EVENT_PHASE_METADATA:
      case TRACE_EVENT_PHASE_COMPLETE:
      case TRACE_EVENT_PHASE_COUNTER:
      case TRACE_EVENT_PHASE_ASYNC_BEGIN:
      case TRACE_EVENT_PHASE_ASYNC_STEP_INTO:
      case TRACE_EVENT_PHASE_ASYNC_END:
        break;
      default:
        // Ignore at this moment. They are not currently used.
        continue;
    }

    if (!event->Validate()) {
      LOG(ERROR) << "Invalid event found " << event->ToString();
      return false;
    }

    parsed_events.emplace_back(std::move(event));
  }

  // Events may come by closure that means event started earlier as a root event
  // for others may appear after children. Sort by ts time.
  std::sort(parsed_events.begin(), parsed_events.end(), SortByTimestampPred);

  for (auto& event : parsed_events) {
    switch (event->GetPhase()) {
      case TRACE_EVENT_PHASE_METADATA:
        metadata_events_.push_back(std::move(event));
        break;
      case TRACE_EVENT_PHASE_ASYNC_BEGIN:
      case TRACE_EVENT_PHASE_ASYNC_STEP_INTO:
      case TRACE_EVENT_PHASE_ASYNC_END:
        group_events_[event->GetId()].push_back(std::move(event));
        break;
      case TRACE_EVENT_PHASE_COMPLETE:
      case TRACE_EVENT_PHASE_COUNTER:
        if (!AddToThread(std::move(event))) {
          LOG(ERROR) << "Cannot add event to threads";
          return false;
        }
        break;
      default:
        NOTREACHED();
        return false;
    }
  }

  return true;
}

bool ArcTracingModel::ConvertSysTraces(const std::string& sys_traces) {
  size_t new_line_pos = 0;

  GraphicsEventsContext graphics_events_context;

  while (true) {
    // Get end of line.
    size_t end_line_pos = sys_traces.find('\n', new_line_pos);
    if (end_line_pos == std::string::npos)
      break;

    const std::string line =
        sys_traces.substr(new_line_pos, end_line_pos - new_line_pos);
    new_line_pos = end_line_pos + 1;

    // Skip comments and empty lines
    if (line.empty() || line[0] == '#')
      continue;

    // Trace event has following format.
    //            TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
    //               | |       |   ||||       |         |
    // Until TIMESTAMP we have fixed position for elements.
    if (line.length() < 35 || line[16] != '-' || line[22] != ' ' ||
        line[23] != '[' || line[27] != ']' || line[28] != ' ' ||
        line[33] != ' ') {
      LOG(ERROR) << "Cannot recognize trace event: " << line;
      return false;
    }

    uint32_t tid;
    if (ParseUint32(line, 17, ' ', &tid) == std::string::npos) {
      LOG(ERROR) << "Cannot parse tid in trace event: " << line;
      return false;
    }

    if (system_model_.thread_map().find(tid) ==
        system_model_.thread_map().end()) {
      int thread_name_start = 0;
      while (line[thread_name_start] == ' ')
        ++thread_name_start;
      system_model_.thread_map()[tid] = ArcSystemModel::ThreadInfo(
          ArcSystemModel::kUnknownPid,
          line.substr(thread_name_start, 16 - thread_name_start));
    }

    uint32_t cpu_id;
    if (ParseUint32(line, 24, ']', &cpu_id) == std::string::npos) {
      LOG(ERROR) << "Cannot parse CPU id in trace event: " << line;
      return false;
    }

    uint32_t timestamp_high;
    uint32_t timestamp_low;
    const size_t pos_dot = ParseUint32(line, 34, '.', &timestamp_high);
    if (pos_dot == std::string::npos) {
      LOG(ERROR) << "Cannot parse timestamp in trace event: " << line;
      return false;
    }
    const size_t separator_position =
        ParseUint32(line, pos_dot + 1, ':', &timestamp_low);
    // We expect to have parsed exactly six digits after the decimal point, to
    // match the scaling factor used just below.
    if (separator_position != pos_dot + 7) {
      LOG(ERROR) << "Cannot parse timestamp in trace event: " << line;
      return false;
    }

    const uint64_t timestamp = 1000000LL * timestamp_high + timestamp_low;
    if (timestamp < min_timestamp_ || timestamp >= max_timestamp_)
      continue;

    if (!strncmp(&line[separator_position], kTracingMarkWrite,
                 kTracingMarkWriteLength)) {
      if (!HandleGraphicsEvent(&graphics_events_context, timestamp, tid, line,
                               separator_position + kTracingMarkWriteLength)) {
        return false;
      }
    } else if (!strncmp(&line[separator_position], kCpuIdle, kCpuIdleLength)) {
      if (!HandleCpuIdle(&system_model_.all_cpu_events(), timestamp, cpu_id,
                         tid, line, separator_position + kCpuIdleLength)) {
        return false;
      }
    } else if (!strncmp(&line[separator_position], kSchedWakeUp,
                        kSchedWakeUpLength)) {
      if (!HandleSchedWakeUp(&system_model_.all_cpu_events(), timestamp, cpu_id,
                             tid, line,
                             separator_position + kSchedWakeUpLength)) {
        return false;
      }
    } else if (!strncmp(&line[separator_position], kSchedSwitch,
                        kSchedSwitchLength)) {
      if (!HandleSchedSwitch(&system_model_.all_cpu_events(), timestamp, cpu_id,
                             tid, line,
                             separator_position + kSchedSwitchLength)) {
        return false;
      }
    } else if (!strncmp(&line[separator_position], kIntelGpuFreqChange,
                        kIntelGpuFreqChangeLength)) {
      if (!HandleGpuFreq(&system_model_.memory_events(), timestamp, line,
                         separator_position + kIntelGpuFreqChangeLength)) {
        return false;
      }
    }
  }

  for (auto& asyncronous_event :
       graphics_events_context.pending_asynchronous_events) {
    group_events_[asyncronous_event.second->GetId()].emplace_back(
        std::move(asyncronous_event.second));
  }

  // Close all pending tracing event, assuming last event is 0 duration.
  for (auto& pending_events :
       graphics_events_context.per_thread_pending_events_stack) {
    if (pending_events.second.empty())
      continue;
    const double last_timestamp = pending_events.second.back()->GetTimestamp();
    for (ArcTracingEvent* pending_event : pending_events.second) {
      pending_event->SetDuration(last_timestamp -
                                 pending_event->GetTimestamp());
      pending_event->SetPhase(TRACE_EVENT_PHASE_COMPLETE);
    }
  }

  // Now put events to the thread models.
  for (auto& converted_event : graphics_events_context.converted_events) {
    if (!AddToThread(std::move(converted_event))) {
      LOG(ERROR) << "Cannot add systrace event to threads";
      return false;
    }
  }

  return true;
}

bool ArcTracingModel::AddToThread(std::unique_ptr<ArcTracingEvent> event) {
  const uint64_t full_id = event->GetPid() * 0x100000000L + event->GetTid();
  std::vector<std::unique_ptr<ArcTracingEvent>>& thread_roots =
      per_thread_events_[full_id];
  if (thread_roots.empty() || thread_roots.back()->ClassifyPositionOf(*event) ==
                                  ArcTracingEvent::Position::kAfter) {
    // First event for the thread or event is after already existing last root
    // event. Add as a new root.
    thread_roots.emplace_back(std::move(event));
    return true;
  }

  return thread_roots.back()->AppendChild(std::move(event));
}

void ArcTracingModel::Dump(std::ostream& stream) const {
  for (const ArcTracingEvent* root : GetRoots())
    root->Dump("", stream);
}

}  // namespace arc
