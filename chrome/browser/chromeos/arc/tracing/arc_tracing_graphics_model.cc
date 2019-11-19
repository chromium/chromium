// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h"

#include <inttypes.h>

#include <algorithm>
#include <set>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/browser/chromeos/arc/tracing/arc_graphics_jank_detector.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event_matcher.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"
#include "components/arc/arc_util.h"
#include "ui/events/event_constants.h"

namespace arc {

namespace {

using BufferEvent = ArcTracingGraphicsModel::BufferEvent;
using BufferEvents = ArcTracingGraphicsModel::BufferEvents;
using BufferEventType = ArcTracingGraphicsModel::BufferEventType;

constexpr char kCustomTracePrefix[] = "customTrace";
constexpr char kInputEventPrefix[] = "InputEvent: ";
constexpr char kDeliverInputEvent[] = "deliverInputEvent";

constexpr char kUnknownActivity[] = "unknown";

constexpr char kArgumentAppId[] = "app_id";
constexpr char kArgumentBufferId[] = "buffer_id";
constexpr char kArgumentPutOffset[] = "put_offset";
constexpr char kArgumentTimestamp[] = "timestamp";
constexpr char kArgumentType[] = "type";

constexpr char kKeyActivity[] = "activity";
constexpr char kKeyAndroid[] = "android";
constexpr char kKeyBuffers[] = "buffers";
constexpr char kKeyChrome[] = "chrome";
constexpr char kKeyDuration[] = "duration";
constexpr char kKeyGlobalEvents[] = "global_events";
constexpr char kKeyIcon[] = "icon";
constexpr char kKeyInformation[] = "information";
constexpr char kKeyInput[] = "input";
constexpr char kKeyViews[] = "views";
constexpr char kKeyPlatform[] = "platform";
constexpr char kKeySystem[] = "system";
constexpr char kKeyTaskId[] = "task_id";
constexpr char kKeyTimestamp[] = "timestamp";
constexpr char kKeyTitle[] = "title";

constexpr char kAcquireBufferQuery[] =
    "android:onMessageReceived/android:handleMessageInvalidate/"
    "android:latchBuffer/android:updateTexImage/android:acquireBuffer";
// Android PI+
constexpr char kReleaseBufferQueryP[] =
    "android:onMessageReceived/android:handleMessageRefresh/"
    "android:postComposition/android:releaseBuffer";
// Android NYC
constexpr char kReleaseBufferQueryN[] =
    "android:onMessageReceived/android:handleMessageRefresh/"
    "android:releaseBuffer";
constexpr char kDequeueBufferQuery[] = "android:dequeueBuffer";
constexpr char kQueueBufferQuery[] = "android:queueBuffer";

constexpr char kInputQuery[] =
    "android:Choreographer#doFrame/android:input/android:";

constexpr char kBarrierOrderingSubQuery[] =
    "gpu:CommandBufferProxyImpl::OrderingBarrier";
constexpr char kBufferInUseQuery[] = "exo:BufferInUse";
constexpr char kHandleMessageRefreshQuery[] =
    "android:onMessageReceived/android:handleMessageRefresh";
constexpr char kHandleMessageInvalidateQuery[] =
    "android:onMessageReceived/android:handleMessageInvalidate";
constexpr char kChromeTopEventsQuery[] =
    "viz,benchmark:Graphics.Pipeline.DrawAndSwap";
constexpr char kSurfaceFlingerVsyncHandlerQuery[] = "android:HW_VSYNC_0|*";
constexpr char kArcVsyncTimestampQuery[] = "android:ARC_VSYNC|*";

constexpr char kBarrierFlushMatcher[] = "gpu:CommandBufferStub::OnAsyncFlush";

constexpr char kExoSurfaceAttachMatcher[] = "exo:Surface::Attach";
constexpr char kExoSurfaceCommitMatcher[] = "exo:Surface::Commit";
constexpr char kExoBufferProduceResourceMatcher[] =
    "exo:Buffer::ProduceTransferableResource";
constexpr char kExoBufferReleaseContentsMatcher[] =
    "exo:Buffer::ReleaseContents";
constexpr char kExoInputEventMatcher[] = "exo:Input::OnInputEvent";

constexpr ssize_t kInvalidBufferIndex = -1;

// Helper factory class that produces graphic buffer events from the giving
// |ArcTracingEvent| generic events. Each |ArcTracingEvent| may produce graphics
// event |ArcTracingGraphicsModel::BufferEvent| on start or/and on finish of the
// event |ArcTracingEvent|. This is organized in form of map
// |ArcTracingEventMatcher| to the pair of |BufferEventType| which indicates
// what to generate on start and on finish of the event.
class BufferGraphicsEventMapper {
 public:
  struct MappingRule {
    using EventTimeCallback =
        base::RepeatingCallback<uint64_t(const ArcTracingEventMatcher&,
                                         const ArcTracingEvent& event)>;

    MappingRule(std::unique_ptr<ArcTracingEventMatcher> matcher,
                BufferEventType map_start,
                BufferEventType map_finish,
                EventTimeCallback start_time_callback = EventTimeCallback())
        : matcher(std::move(matcher)),
          map_start(map_start),
          map_finish(map_finish),
          event_start_time_callback(start_time_callback) {}

    bool Produce(const ArcTracingEvent& event,
                 ArcTracingGraphicsModel::BufferEvents* collector) const {
      if (!matcher->Match(event))
        return false;

      if (map_start != BufferEventType::kNone) {
        uint64_t start_timestamp = event.GetTimestamp();
        if (event_start_time_callback) {
          start_timestamp = event_start_time_callback.Run(*matcher, event);
        }

        collector->push_back(
            ArcTracingGraphicsModel::BufferEvent(map_start, start_timestamp));
      }
      if (map_finish != BufferEventType::kNone) {
        collector->push_back(ArcTracingGraphicsModel::BufferEvent(
            map_finish, event.GetEndTimestamp()));
      }

      return true;
    }

    std::unique_ptr<ArcTracingEventMatcher> matcher;
    BufferEventType map_start;
    BufferEventType map_finish;
    EventTimeCallback event_start_time_callback;
  };
  using MappingRules = std::vector<MappingRule>;

  BufferGraphicsEventMapper() {
    // exo rules
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(kExoSurfaceAttachMatcher),
        BufferEventType::kExoSurfaceAttach, BufferEventType::kNone));
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(kExoSurfaceCommitMatcher),
        BufferEventType::kNone, BufferEventType::kExoSurfaceCommit));
    rules_.emplace_back(MappingRule(std::make_unique<ArcTracingEventMatcher>(
                                        kExoBufferProduceResourceMatcher),
                                    BufferEventType::kExoProduceResource,
                                    BufferEventType::kNone));
    rules_.emplace_back(MappingRule(std::make_unique<ArcTracingEventMatcher>(
                                        kExoBufferReleaseContentsMatcher),
                                    BufferEventType::kNone,
                                    BufferEventType::kExoReleased));
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>("exo:BufferInUse(step=bound)"),
        BufferEventType::kExoBound, BufferEventType::kNone));
    rules_.emplace_back(MappingRule(std::make_unique<ArcTracingEventMatcher>(
                                        "exo:BufferInUse(step=pending_query)"),
                                    BufferEventType::kExoPendingQuery,
                                    BufferEventType::kNone));

    // gpu rules
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(
            "gpu:CommandBufferProxyImpl::OrderingBarrier"),
        BufferEventType::kChromeBarrierOrder, BufferEventType::kNone));
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(kBarrierFlushMatcher),
        BufferEventType::kNone, BufferEventType::kChromeBarrierFlush));

    // android rules
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(kDequeueBufferQuery),
        BufferEventType::kBufferQueueDequeueStart,
        BufferEventType::kBufferQueueDequeueDone));
    rules_.emplace_back(
        MappingRule(std::make_unique<ArcTracingEventMatcher>(kQueueBufferQuery),
                    BufferEventType::kBufferQueueQueueStart,
                    BufferEventType::kBufferQueueQueueDone));
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>("android:acquireBuffer"),
        BufferEventType::kBufferQueueAcquire, BufferEventType::kNone));
    rules_.push_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>("android:releaseBuffer"),
        BufferEventType::kNone, BufferEventType::kBufferQueueReleased));
    rules_.emplace_back(
        MappingRule(std::make_unique<ArcTracingEventMatcher>(
                        "android:handleMessageInvalidate"),
                    BufferEventType::kSurfaceFlingerInvalidationStart,
                    BufferEventType::kSurfaceFlingerInvalidationDone));
    rules_.emplace_back(
        MappingRule(std::make_unique<ArcTracingEventMatcher>(
                        "android:handleMessageRefresh"),
                    BufferEventType::kSurfaceFlingerCompositionStart,
                    BufferEventType::kSurfaceFlingerCompositionDone));

    // viz,benchmark rules
    auto matcher = std::make_unique<ArcTracingEventMatcher>(
        "viz,benchmark:Graphics.Pipeline.DrawAndSwap");
    matcher->SetPhase(TRACE_EVENT_PHASE_ASYNC_BEGIN);
    rules_.emplace_back(MappingRule(std::move(matcher),
                                    BufferEventType::kChromeOSDraw,
                                    BufferEventType::kNone));
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(
            "viz,benchmark:Graphics.Pipeline.DrawAndSwap(step=Draw)"),
        BufferEventType::kNone, BufferEventType::kNone));
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(
            "viz,benchmark:Graphics.Pipeline.DrawAndSwap(step=Swap)"),
        BufferEventType::kChromeOSSwap, BufferEventType::kNone));
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(
            "viz,benchmark:Graphics.Pipeline.DrawAndSwap(step=WaitForAck)"),
        BufferEventType::kChromeOSWaitForAck, BufferEventType::kNone));
    rules_.emplace_back(MappingRule(
        std::make_unique<ArcTracingEventMatcher>(
            "viz,benchmark:Graphics.Pipeline.DrawAndSwap(step="
            "WaitForPresentation)"),
        BufferEventType::kChromeOSPresentationDone, BufferEventType::kNone));
    matcher = std::make_unique<ArcTracingEventMatcher>(
        "viz,benchmark:Graphics.Pipeline.DrawAndSwap");
    matcher->SetPhase(TRACE_EVENT_PHASE_ASYNC_END);
    rules_.emplace_back(MappingRule(std::move(matcher), BufferEventType::kNone,
                                    BufferEventType::kChromeOSSwapDone));
  }

  ~BufferGraphicsEventMapper() = default;

  void Produce(const ArcTracingEvent& event,
               ArcTracingGraphicsModel::BufferEvents* collector) const {
    for (const auto& rule : rules_) {
      if (rule.Produce(event, collector))
        return;
    }
    LOG(ERROR) << "Unsupported event: " << event.ToString();
  }

 private:
  MappingRules rules_;

  DISALLOW_COPY_AND_ASSIGN(BufferGraphicsEventMapper);
};

BufferGraphicsEventMapper& GetEventMapper() {
  static base::NoDestructor<BufferGraphicsEventMapper> instance;
  return *instance;
}

// Maps particular buffer to its events.
using BufferToEvents =
    std::map<std::string, ArcTracingGraphicsModel::BufferEvents>;

bool SortByTimestampPred(const ArcTracingGraphicsModel::BufferEvent& a,
                         const ArcTracingGraphicsModel::BufferEvent& b) {
  if (a.timestamp != b.timestamp)
    return a.timestamp < b.timestamp;
  return static_cast<int>(a.type) < static_cast<int>(b.type);
}

void SortBufferEventsByTimestamp(BufferEvents* events) {
  std::sort(events->begin(), events->end(), SortByTimestampPred);
}

std::string RouteToSelector(const std::vector<const ArcTracingEvent*>& route) {
  std::string result;
  for (const ArcTracingEvent* segment : route)
    result = result + "/" + segment->GetCategory() + ":" + segment->GetName();
  return result;
}

void DetermineHierarchy(std::vector<const ArcTracingEvent*>* route,
                        const ArcTracingEvent* event,
                        const ArcTracingEventMatcher& matcher,
                        std::string* out_query) {
  if (!out_query->empty())
    return;

  route->emplace_back(event);

  if (matcher.Match(*event)) {
    *out_query = RouteToSelector(*route);
  } else {
    for (const auto& child : event->children())
      DetermineHierarchy(route, child.get(), matcher, out_query);
  }

  route->pop_back();
}

// Extracts buffer id from the surface flinger event. For example:
// android|releaseBuffer
//   android|com.android.vending/com.android.vending.AssetBrowserActivity#0: 2
// Buffer id appears as a child event where name if the combination of the
// current view of the Activity, its index and the number of buffer starting
// from 0. This helps to exactly identify the particular buffer in context of
// Android. Buffer id for this example is
// "com.android.vending/com.android.vending.AssetBrowserActivity#0: 2"
bool ExtractBufferIdFromSurfaceFlingerEvent(const ArcTracingEvent& event,
                                            std::string* id) {
  for (const auto& child : event.children()) {
    if (child->GetPhase() != TRACE_EVENT_PHASE_COMPLETE)
      continue;
    const std::string& name = child->GetName();
    size_t index = name.find(": ");
    if (index == std::string::npos)
      continue;
    index += 2;
    if (index >= name.length())
      continue;
    bool all_digits = true;
    while (index < name.length() && all_digits) {
      all_digits &= (name[index] >= '0' && name[index] <= '9');
      ++index;
    }
    if (!all_digits)
      continue;
    *id = name;
    return true;
  }
  return false;
}

// Extracts the activity name from the buffer id by discarding the buffer id
// and view index. For example, activity name for buffer id
// "com.android.vending/com.android.vending.AssetBrowserActivity#0: 2"
// is "com.android.vending/com.android.vending.AssetBrowserActivity".
// If the activity cannot be extracted then default |kUnknownActivity| is
// returned.
std::string GetActivityFromBufferName(const std::string& android_buffer_name) {
  const size_t position = android_buffer_name.find('#');
  if (position == std::string::npos)
    return kUnknownActivity;
  return android_buffer_name.substr(0, position);
}

// Processes surface flinger events. It selects events using |query| from the
// model. Buffer id is extracted for the each returned event and new events are
// grouped by its buffer id. If |surface_flinger_pid| is set to the positive
// value than this activates filtering events by process id to avoid the case
// when android:queueBuffer/dequeueBuffer events may appear in context of child
// process and could be processed as surface flinger event. Returns positive
// process id in case surface flinger events are found and belong to the same
// process. In case of error -1 is returned.
int ProcessSurfaceFlingerEvents(const ArcTracingModel& common_model,
                                const std::string& query,
                                BufferToEvents* buffer_to_events,
                                int surface_flinger_pid) {
  int detected_pid = -1;
  const ArcTracingModel::TracingEventPtrs surface_flinger_events =
      common_model.Select(query);
  std::string buffer_id;
  for (const ArcTracingEvent* event : surface_flinger_events) {
    if (surface_flinger_pid > 0 && event->GetPid() != surface_flinger_pid)
      continue;
    if (!ExtractBufferIdFromSurfaceFlingerEvent(*event, &buffer_id)) {
      LOG(ERROR) << "Failed to get buffer id from surface flinger event";
      continue;
    }
    if (detected_pid < 0) {
      DCHECK_GE(event->GetPid(), 0);
      detected_pid = event->GetPid();
    } else if (detected_pid != event->GetPid()) {
      LOG(ERROR) << "Found multiple surface flinger process ids "
                 << detected_pid << "/" << event->GetPid();
      return -1;
    }
    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        (*buffer_to_events)[buffer_id];
    GetEventMapper().Produce(*event, &graphics_events);
  }
  return detected_pid;
}

// Processes Android events acquireBuffer, releaseBuffer, dequeueBuffer and
// queueBuffer. It returns map buffer id to the list of sorted by timestamp
// events.
bool GetSurfaceFlingerEvents(const ArcTracingModel& common_model,
                             BufferToEvents* out_events) {
  // Detect surface_flinger_pid using |kAcquireBufferQuery| that has unique
  // hierarchy.
  const int surface_flinger_pid =
      ProcessSurfaceFlingerEvents(common_model, kAcquireBufferQuery, out_events,
                                  -1 /* surface_flinger_pid */);
  if (surface_flinger_pid <= 0) {
    LOG(ERROR) << "Failed to detect acquireBuffer events.";
    return false;
  }

  const int surface_flinger_pid_p =
      ProcessSurfaceFlingerEvents(common_model, kReleaseBufferQueryP,
                                  out_events, -1 /* surface_flinger_pid */);
  const int surface_flinger_pid_n =
      ProcessSurfaceFlingerEvents(common_model, kReleaseBufferQueryN,
                                  out_events, -1 /* surface_flinger_pid */);
  if (surface_flinger_pid_p <= 0 && surface_flinger_pid_n <= 0) {
    LOG(ERROR) << "Failed to detect releaseBuffer events.";
    return false;
  }

  if (surface_flinger_pid_p > 0 && surface_flinger_pid_n > 0) {
    LOG(ERROR) << "Detected releaseBuffer events from both NYC and PI.";
    return false;
  }

  if (surface_flinger_pid_p != surface_flinger_pid &&
      surface_flinger_pid_n != surface_flinger_pid) {
    LOG(ERROR) << "Detected acquireBuffer and releaseBuffer from"
                  " different processes.";
    return false;
  }

  // queueBuffer and dequeueBuffer may appear in context of client task.
  // Use detected |surface_flinger_pid| to filter out such events.
  if (ProcessSurfaceFlingerEvents(common_model, kQueueBufferQuery, out_events,
                                  surface_flinger_pid) < 0) {
    LOG(ERROR) << "Failed to detect queueBuffer events.";
    return false;
  }

  if (ProcessSurfaceFlingerEvents(common_model, kDequeueBufferQuery, out_events,
                                  surface_flinger_pid) < 0) {
    LOG(ERROR) << "Failed to detect dequeueBuffer events.";
    return false;
  }

  for (auto& buffer : *out_events)
    SortBufferEventsByTimestamp(&buffer.second);
  return true;
}

// Represents input event in Wayland. It contains input timestamp that shows the
// time of event creation, event timestamp in this case shows the time when this
// event was dispatched using Wayland interface. Type defines the input type.
class ExoInputEvent {
 public:
  ExoInputEvent(const ArcTracingEvent* event,
                uint64_t input_timestamp,
                ui::EventType type)
      : event_(event), input_timestamp_(input_timestamp), type_(type) {}
  ~ExoInputEvent() = default;

  // Parses |event| and extracts information for Wayland input event. Returns
  // nullptr in case |event| could not be parsed.
  static std::unique_ptr<ExoInputEvent> Create(const ArcTracingEvent* event) {
    const uint64_t timestamp =
        event->GetArgAsDouble(kArgumentTimestamp, 0 /* default_value */);
    const int type =
        event->GetArgAsInteger(kArgumentType, 0 /* default_value */);

    if (!timestamp || !type) {
      LOG(ERROR) << "Could not parse timestamp or type of event: "
                 << event->ToString();
      return nullptr;
    }

    return std::make_unique<ExoInputEvent>(event, timestamp,
                                           static_cast<ui::EventType>(type));
  }

  const ArcTracingEvent* event() const { return event_; }
  uint64_t input_timestamp() const { return input_timestamp_; }
  ui::EventType type() const { return type_; }

 private:
  const ArcTracingEvent* event_;
  // Time of the creation of the event. Normally, it is before the event
  // timestamp that indicates when event was seen in Wayland.
  const uint64_t input_timestamp_;
  // Type of the event;
  const ui::EventType type_;

  DISALLOW_COPY_AND_ASSIGN(ExoInputEvent);
};

bool SortExoByInputTimestampPred(const std::unique_ptr<ExoInputEvent>& a,
                                 const uint64_t input_timestamp) {
  return a->input_timestamp() < input_timestamp;
}

// Represents input event in Android. It contains input timestamp that shows the
// time of the creation event in Chrome. Source defines the origin of the input
// event, mouse, keyboard, touch and so on. Sequence id is the unique
// identifier of the input event in context of one application.
class AndroidInputEvent {
 public:
  enum Source {
    Keyboard = 0x101,
    Touch = 0x1002,
    Mouse = 0x2002,
  };

  AndroidInputEvent(const ArcTracingEvent* event,
                    std::vector<uint64_t> input_timestamps,
                    Source source,
                    int sequence_id)
      : event_(event),
        input_timestamps_(std::move(input_timestamps)),
        source_(source),
        sequence_id_(sequence_id) {}

  ~AndroidInputEvent() = default;

  // Parses |event| and extracts information for Android input event. Returns
  // nullptr in case |event| could not be parsed.
  static std::unique_ptr<AndroidInputEvent> Create(
      const ArcTracingEvent* event) {
    if (!base::StartsWith(event->GetName(), kInputEventPrefix,
                          base::CompareCase::SENSITIVE)) {
      return nullptr;
    }

    // Has following structure
    // InputEvent: source timestamps sequence_id|0|
    const std::string body =
        event->GetName().substr(base::size(kInputEventPrefix) - 1);
    base::StringTokenizer tokenizer(body, " ");
    std::vector<std::string> tokens;
    while (tokenizer.GetNext())
      tokens.emplace_back(tokenizer.token());
    if (tokens.size() != 3) {
      LOG(ERROR) << "Failed to parse input event: " << event->ToString();
      return nullptr;
    }

    uint32_t source = -1;
    sscanf(tokens[0].c_str(), "%" PRId32, &source);
    switch (static_cast<Source>(source)) {
      case Source::Keyboard:
      case Source::Mouse:
      case Source::Touch:
        break;
      default:
        LOG(ERROR) << "Unrecognized source: " << event->ToString();
        return nullptr;
    }

    std::vector<uint64_t> input_timestamps;
    // Timestamps are separated by comma. It is used in case event is synthetic
    // and is composed from one more physical events.
    base::StringTokenizer tokenizer_timestamps(tokens[1], ",");
    while (tokenizer_timestamps.GetNext()) {
      uint64_t timestamp;
      if (sscanf(tokenizer_timestamps.token().c_str(), "%" PRId64,
                 &timestamp) != 1) {
        LOG(ERROR) << "Failed to parse timestamp: " << event->ToString();
        return nullptr;
      }
      // Convert nanosecond to microseconds, as it is use in tracing.
      input_timestamps.emplace_back(timestamp / 1000L);
    }

    if (input_timestamps.empty()) {
      LOG(ERROR) << "Timestamp is not found: " << event->ToString();
      return nullptr;
    }

    int sequence_id;
    if (sscanf(tokens[2].c_str(), "%" PRId32 "|0|", &sequence_id) != 1) {
      LOG(ERROR) << "Sequence id is not found: " << event->ToString();
      return nullptr;
    }

    return std::make_unique<AndroidInputEvent>(
        event, std::move(input_timestamps), static_cast<Source>(source),
        sequence_id);
  }

  const ArcTracingEvent* event() const { return event_; }
  std::vector<uint64_t> input_timestamps() const { return input_timestamps_; }
  Source source() const { return source_; }
  int sequence_id() const { return sequence_id_; }

 private:
  const ArcTracingEvent* event_;
  // Time of the creation of the event. Note that Wayland passes only
  // milliseconds. So for events coming from Chrome, it is expected 0 for
  // microsecond and nanosecond fraction. There is special case for motion
  // events, when Android motion event is composed from the set of actual
  // events with prediction of trajectory. In last case |input_timestamps_|
  // contains all events, used for calculation of synthetic move event.
  const std::vector<uint64_t> input_timestamps_;

  const Source source_;
  const int sequence_id_;

  DISALLOW_COPY_AND_ASSIGN(AndroidInputEvent);
};

// Maps Android sources to possible input types from Chrome.
struct InputEventMapper {
  InputEventMapper() {
    source_to_type[AndroidInputEvent::Source::Keyboard] = {
        ui::EventType::ET_KEY_PRESSED, ui::EventType::ET_KEY_RELEASED};
    source_to_type[AndroidInputEvent::Source::Touch] = {
        ui::EventType::ET_TOUCH_RELEASED, ui::EventType::ET_TOUCH_PRESSED,
        ui::EventType::ET_TOUCH_MOVED, ui::EventType::ET_TOUCH_CANCELLED};
    source_to_type[AndroidInputEvent::Source::Mouse] = {
        ui::EventType::ET_MOUSE_PRESSED,
        ui::EventType::ET_MOUSE_RELEASED,
        ui::EventType::ET_MOUSE_MOVED,
    };
  }

  std::map<int, std::set<ui::EventType>> source_to_type;
};

const InputEventMapper& GetInputEventMapper() {
  static base::NoDestructor<InputEventMapper> instance;
  return *instance;
}

// Finds the corresponded Wayland input event among |exo_input_events| for the
// given |android_input_timestamp| and |source|. Return nullptr in case event
// could not be found or more than one event matches the
// |android_input_timestamp| and |source|. |exo_input_events| is sorted by input
// timestamp.
const ExoInputEvent* FindExoInputEventForAndroidEvent(
    const std::vector<std::unique_ptr<ExoInputEvent>>& exo_input_events,
    uint64_t android_input_timestamp,
    int source) {
  const InputEventMapper& mapper = GetInputEventMapper();
  const auto& allowed_set = mapper.source_to_type.find(source);
  if (allowed_set == mapper.source_to_type.end()) {
    LOG(ERROR) << "Input source is not recognized " << source;
    return nullptr;
  }

  // Wayland does not pass microsecond and nanosecond fraction. If it set, that
  // means we deal with synthetic Android input event and does not have match in
  // Wayland.
  if (android_input_timestamp % 1000L)
    return nullptr;

  const ExoInputEvent* exo_input_event = nullptr;
  // Seek in the range of 1 millisecond once |android_input_timestamp| has
  // millisecond resolution.
  const uint64_t max_input_timestamp = android_input_timestamp + 1000L;
  auto it =
      std::lower_bound(exo_input_events.begin(), exo_input_events.end(),
                       android_input_timestamp, SortExoByInputTimestampPred);
  while (it != exo_input_events.end() &&
         it->get()->input_timestamp() < max_input_timestamp) {
    if (allowed_set->second.count(it->get()->type())) {
      // Check if detected more than one exo input event.
      if (exo_input_event) {
        LOG(WARNING) << "More than one exo input event found for timestamp "
                     << android_input_timestamp;
        return nullptr;
      }
      exo_input_event = it->get();
    }
    ++it;
  }

  return exo_input_event;
}

// Finds the timestamps when input event was delivered to the target app. This
// finds the pair of asynchronous events deliverInputEvent that has the same
// sequence id and belongs to the same process as |input_event|. Returns true
// in case |out_start_delivery_timestamp| and |out_end_delivery_timestamp| are
// set.
bool GetDeliverInputEventTimestamp(const ArcTracingModel& common_model,
                                   const AndroidInputEvent* input_event,
                                   uint64_t* out_start_delivery_timestamp,
                                   uint64_t* out_end_delivery_timestamp) {
  const ArcTracingModel::TracingEventPtrs group_events =
      common_model.GetGroupEvents(
          base::StringPrintf("%d", input_event->sequence_id()));

  const ArcTracingEvent* start_event = nullptr;
  const ArcTracingEvent* end_event = nullptr;
  for (const ArcTracingEvent* event : group_events) {
    if (event->GetName() != kDeliverInputEvent)
      continue;
    // Potentially events from different tasks/processes may overlap.
    if (event->GetPid() != input_event->event()->GetPid())
      continue;

    switch (event->GetPhase()) {
      case TRACE_EVENT_PHASE_ASYNC_BEGIN:
        if (start_event) {
          LOG(ERROR) << "Double start event found " << start_event->ToString();
          return false;
        }
        start_event = event;
        break;
      case TRACE_EVENT_PHASE_ASYNC_END:
        if (end_event) {
          LOG(ERROR) << "Double end event found " << end_event->ToString();
          return false;
        }
        end_event = event;
        break;
    }
  }

  if (!start_event || !end_event)
    return false;

  *out_start_delivery_timestamp = start_event->GetTimestamp();
  *out_end_delivery_timestamp = end_event->GetTimestamp();

  if (*out_start_delivery_timestamp > *out_end_delivery_timestamp) {
    LOG(ERROR) << "Start event is after end event " << start_event->ToString();
    return false;
  }

  if (*out_start_delivery_timestamp > input_event->event()->GetTimestamp() ||
      *out_end_delivery_timestamp < input_event->event()->GetTimestamp()) {
    LOG(ERROR) << "Wrong start/end timestamps for "
               << input_event->event()->ToString();
    return false;
  }

  return true;
}

std::string InputTypeToString(ui::EventType type) {
  switch (type) {
    case ui::EventType::ET_MOUSE_PRESSED:
      return "mouse pressed";
    case ui::EventType::ET_MOUSE_RELEASED:
      return "mouse released";
    case ui::EventType::ET_MOUSE_MOVED:
      return "mouse moved";
    case ui::EventType::ET_KEY_PRESSED:
      return "key pressed";
    case ui::EventType::ET_KEY_RELEASED:
      return "key released";
    case ui::EventType::ET_TOUCH_RELEASED:
      return "touch released";
    case ui::EventType::ET_TOUCH_PRESSED:
      return "touch pressed";
    case ui::EventType::ET_TOUCH_MOVED:
      return "touch moved";
    default:
      return base::StringPrintf("type: %d", type);
  }
}

std::string SourceToString(AndroidInputEvent::Source source) {
  switch (source) {
    case AndroidInputEvent::Source::Keyboard:
      return "keyboard";
    case AndroidInputEvent::Source::Mouse:
      return "mouse";
    case AndroidInputEvent::Source::Touch:
      return "touch";
    default:
      return base::StringPrintf("source: %d", static_cast<int>(source));
  }
}

// Analyzes |common_model| and reconstructs input events activity.
void GetInputEvents(
    const ArcTracingModel& common_model,
    ArcTracingGraphicsModel::EventsContainer* out_events_container) {
  // Determine route to Wayland input events.
  const ArcTracingModel::TracingEventPtrs top_level_events =
      common_model.Select("toplevel:");
  std::vector<const ArcTracingEvent*> route;
  std::string exo_input_event_query;
  for (const ArcTracingEvent* top_level_event : top_level_events) {
    DetermineHierarchy(&route, top_level_event,
                       ArcTracingEventMatcher(kExoInputEventMatcher),
                       &exo_input_event_query);
  }

  // Collect list of input events, seen in Wayaland and sort them based on input
  // event creation time.
  std::vector<std::unique_ptr<ExoInputEvent>> exo_input_events;
  if (!exo_input_event_query.empty()) {
    const ArcTracingModel::TracingEventPtrs input_events =
        common_model.Select(exo_input_event_query);
    for (const ArcTracingEvent* input_event : input_events) {
      std::unique_ptr<ExoInputEvent> exo_input_event =
          ExoInputEvent::Create(input_event);
      if (exo_input_event)
        exo_input_events.emplace_back(std::move(exo_input_event));
    }
  }

  std::sort(exo_input_events.begin(), exo_input_events.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs->input_timestamp() != rhs->input_timestamp())
                return lhs->input_timestamp() < rhs->input_timestamp();
              return lhs->event()->GetTimestamp() <
                     rhs->event()->GetTimestamp();
            });

  // Extracts Android input events that can appear as root events or under the
  // |kInputQuery| hierarchy.
  std::vector<std::unique_ptr<AndroidInputEvent>> android_input_events;
  ArcTracingModel::TracingEventPtrs android_input_event_candidates =
      common_model.GetRoots();
  const ArcTracingModel::TracingEventPtrs inputs =
      common_model.Select(kInputQuery);
  android_input_event_candidates.insert(android_input_event_candidates.end(),
                                        inputs.begin(), inputs.end());
  for (const ArcTracingEvent* root : android_input_event_candidates) {
    // It is logged as counter.
    if (root->GetPhase() != TRACE_EVENT_PHASE_COUNTER)
      continue;

    std::unique_ptr<AndroidInputEvent> android_input_event =
        AndroidInputEvent::Create(root);
    if (android_input_event)
      android_input_events.emplace_back(std::move(android_input_event));
  }

  std::sort(android_input_events.begin(), android_input_events.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs->input_timestamps()[0] != rhs->input_timestamps()[0])
                return lhs->input_timestamps()[0] < rhs->input_timestamps()[0];
              return lhs->event()->GetTimestamp() <
                     rhs->event()->GetTimestamp();
            });

  // Group of events per each input.
  std::vector<BufferEvents> events_group;
  for (const auto& android_input_event : android_input_events) {
    uint64_t start_delivery_timestamp;
    uint64_t end_delivery_timestamp;
    if (!GetDeliverInputEventTimestamp(common_model, android_input_event.get(),
                                       &start_delivery_timestamp,
                                       &end_delivery_timestamp)) {
      LOG(ERROR) << "Deliver input event is not found for "
                 << android_input_event->event()->ToString();
      continue;
    }

    BufferEvents input_events;
    input_events.emplace_back(BufferEventType::kInputEventDeliverStart,
                              start_delivery_timestamp);
    input_events.emplace_back(BufferEventType::kInputEventDeliverEnd,
                              end_delivery_timestamp);
    // In case of synthetic move event, last timestamp is generated timestamp,
    // discard it.
    constexpr size_t one = 1;
    const size_t timestamp_count =
        std::max(one, android_input_event->input_timestamps().size() - 1);
    for (size_t i = 0; i < timestamp_count; ++i) {
      const uint64_t input_timestamp =
          android_input_event->input_timestamps()[i];
      const ExoInputEvent* exo_input_event = FindExoInputEventForAndroidEvent(
          exo_input_events, input_timestamp, android_input_event->source());
      if (exo_input_event) {
        const uint64_t dispatch_timestamp =
            exo_input_event->event()->GetTimestamp();
        // crbug.com/968324, input timestamp might be set in the future, in this
        // case use dispatch_timestamp as creation timestamp.
        const uint64_t creation_timestamp = exo_input_event->input_timestamp();
        if (creation_timestamp <= dispatch_timestamp) {
          input_events.emplace_back(BufferEventType::kInputEventCreated,
                                    creation_timestamp,
                                    InputTypeToString(exo_input_event->type()));
        } else {
          // Sort by type.
          input_events.emplace_back(
              BufferEventType::kInputEventCreated, dispatch_timestamp,
              InputTypeToString(exo_input_event->type()) + " - estimated");
        }

        input_events.emplace_back(BufferEventType::kInputEventWaylandDispatched,
                                  dispatch_timestamp);
      } else {
        // Could not map Wayland event. Let use best we can, which precision
        // rounded down to millisecond.
        // crbug.com/968324, input timestamp might be set in the future, in this
        // case, clamp to |start_delivery_timestamp|.
        if (input_timestamp <= start_delivery_timestamp) {
          input_events.emplace_back(
              BufferEventType::kInputEventCreated, input_timestamp,
              SourceToString(android_input_event->source()));
        } else {
          input_events.emplace_back(
              BufferEventType::kInputEventCreated, start_delivery_timestamp,
              SourceToString(android_input_event->source()) + " - estimated");
        }
      }
    }
    SortBufferEventsByTimestamp(&input_events);
    DCHECK_EQ(BufferEventType::kInputEventCreated, input_events.begin()->type);
    DCHECK_EQ(BufferEventType::kInputEventDeliverEnd,
              input_events.rbegin()->type);

    // Try to put series of events to the default first bar. However, in case
    // series of events overlap, use another bar to keep them separated.
    size_t target_buffer = 0;
    for (target_buffer = 0;
         target_buffer < out_events_container->buffer_events().size();
         ++target_buffer) {
      if (out_events_container->buffer_events()[target_buffer]
              .rbegin()
              ->timestamp < input_events.begin()->timestamp) {
        break;
      }
    }
    if (target_buffer == out_events_container->buffer_events().size())
      out_events_container->buffer_events().emplace_back(BufferEvents());

    out_events_container->buffer_events()[target_buffer].insert(
        out_events_container->buffer_events()[target_buffer].end(),
        input_events.begin(), input_events.end());
  }
}

// Processes exo events Surface::Attach and Buffer::ReleaseContents. Each event
// has argument buffer_id that identifies graphics buffer on Chrome side.
// buffer_id is just row pointer to internal class. If |buffer_id_to_task_id| is
// set then it is updated to map buffer id to task id.
void ProcessChromeEvents(const ArcTracingModel& common_model,
                         const std::string& query,
                         BufferToEvents* buffer_to_events,
                         std::map<std::string, int>* buffer_id_to_task_id) {
  const ArcTracingModel::TracingEventPtrs chrome_events =
      common_model.Select(query);
  for (const ArcTracingEvent* event : chrome_events) {
    const std::string buffer_id = event->GetArgAsString(
        kArgumentBufferId, std::string() /* default_value */);
    if (buffer_id.empty()) {
      LOG(ERROR) << "Failed to get buffer id from event: " << event->ToString();
      continue;
    }
    if (buffer_id_to_task_id) {
      const std::string app_id = event->GetArgAsString(
          kArgumentAppId, std::string() /* default_value */);
      if (app_id.empty()) {
        LOG(ERROR) << "Failed to get app id from event: " << event->ToString();
        continue;
      }
      int task_id = GetTaskIdFromWindowAppId(app_id);
      if (task_id == kNoTaskId) {
        LOG(ERROR) << "Failed to parse app id from event: "
                   << event->ToString();
        continue;
      }
      (*buffer_id_to_task_id)[buffer_id] = task_id;
    }
    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        (*buffer_to_events)[buffer_id];
    GetEventMapper().Produce(*event, &graphics_events);
  }
}

BufferToEvents GetChromeEvents(
    const ArcTracingModel& common_model,
    std::map<std::string, int>* buffer_id_to_task_id) {
  // The tracing hierarchy may be easy changed any time in Chrome. This makes
  // using static queries fragile and dependent of many external components. To
  // provide the reliable way of requesting the needed information, let scan
  // |common_model| for top level events and determine the hierarchy of
  // interesting events dynamically.
  const ArcTracingModel::TracingEventPtrs top_level_events =
      common_model.Select("toplevel:");
  std::vector<const ArcTracingEvent*> route;
  std::string barrier_flush_query;
  const ArcTracingEventMatcher barrier_flush_matcher(kBarrierFlushMatcher);
  std::string attach_surface_query;
  const ArcTracingEventMatcher attach_surface_matcher(kExoSurfaceAttachMatcher);
  std::string commit_surface_query;
  const ArcTracingEventMatcher commit_surface_matcher(kExoSurfaceCommitMatcher);
  std::string produce_resource_query;
  const ArcTracingEventMatcher produce_resource_matcher(
      kExoBufferProduceResourceMatcher);
  std::string release_contents_query;
  const ArcTracingEventMatcher release_contents_matcher(
      kExoBufferReleaseContentsMatcher);
  for (const ArcTracingEvent* top_level_event : top_level_events) {
    DetermineHierarchy(&route, top_level_event, barrier_flush_matcher,
                       &barrier_flush_query);
    DetermineHierarchy(&route, top_level_event, attach_surface_matcher,
                       &attach_surface_query);
    DetermineHierarchy(&route, top_level_event, commit_surface_matcher,
                       &commit_surface_query);
    DetermineHierarchy(&route, top_level_event, produce_resource_matcher,
                       &produce_resource_query);
    DetermineHierarchy(&route, top_level_event, release_contents_matcher,
                       &release_contents_query);
  }

  BufferToEvents per_buffer_chrome_events;
  // Only exo:Surface::Attach has app id argument.
  ProcessChromeEvents(common_model, attach_surface_query,
                      &per_buffer_chrome_events, buffer_id_to_task_id);
  ProcessChromeEvents(common_model, commit_surface_query,
                      &per_buffer_chrome_events,
                      nullptr /* buffer_id_to_task_id */);
  ProcessChromeEvents(common_model, release_contents_query,
                      &per_buffer_chrome_events,
                      nullptr /* buffer_id_to_task_id */);

  // Handle ProduceTransferableResource events. They have extra link to barrier
  // events. Use buffer_id to bind events for the same graphics buffer.
  const ArcTracingModel::TracingEventPtrs produce_resource_events =
      common_model.Select(produce_resource_query);
  std::map<int, std::string> put_offset_to_buffer_id_map;
  for (const ArcTracingEvent* event : produce_resource_events) {
    const std::string buffer_id = event->GetArgAsString(
        kArgumentBufferId, std::string() /* default_value */);
    if (buffer_id.empty()) {
      LOG(ERROR) << "Failed to get buffer id from event: " << event->ToString();
      continue;
    }

    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        per_buffer_chrome_events[buffer_id];
    GetEventMapper().Produce(*event, &graphics_events);

    const ArcTracingModel::TracingEventPtrs ordering_barrier_events =
        common_model.Select(event, kBarrierOrderingSubQuery);
    if (ordering_barrier_events.size() != 1) {
      LOG(ERROR) << "Expected only one " << kBarrierOrderingSubQuery << ". Got "
                 << ordering_barrier_events.size();
      continue;
    }
    const int put_offset = ordering_barrier_events[0]->GetArgAsInteger(
        kArgumentPutOffset, 0 /* default_value */);
    if (!put_offset) {
      LOG(ERROR) << "No " << kArgumentPutOffset
                 << " argument in: " << ordering_barrier_events[0]->ToString();
      continue;
    }
    if (put_offset_to_buffer_id_map.count(put_offset) &&
        put_offset_to_buffer_id_map[put_offset] != buffer_id) {
      LOG(ERROR) << put_offset << " is already mapped to "
                 << put_offset_to_buffer_id_map[put_offset]
                 << ". Skip mapping to " << buffer_id;
      continue;
    }
    put_offset_to_buffer_id_map[put_offset] = buffer_id;
    GetEventMapper().Produce(*ordering_barrier_events[0], &graphics_events);
  }

  // Find associated barrier flush event using put_offset argument.
  const ArcTracingModel::TracingEventPtrs barrier_flush_events =
      common_model.Select(barrier_flush_query);
  for (const ArcTracingEvent* event : barrier_flush_events) {
    const int put_offset =
        event->GetArgAsInteger(kArgumentPutOffset, 0 /* default_value */);
    if (!put_offset_to_buffer_id_map.count(put_offset))
      continue;
    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        per_buffer_chrome_events[put_offset_to_buffer_id_map[put_offset]];
    GetEventMapper().Produce(*event, &graphics_events);
  }

  // Handle BufferInUse async events.
  const ArcTracingModel::TracingEventPtrs buffer_in_use_events =
      common_model.Select(kBufferInUseQuery);
  std::map<std::string, std::string> buffer_in_use_id_to_buffer_id;
  for (const ArcTracingEvent* event : buffer_in_use_events) {
    // Only start event has buffer_id association.
    if (event->GetPhase() != TRACE_EVENT_PHASE_ASYNC_BEGIN)
      continue;
    const std::string id = event->GetId();
    const std::string buffer_id = event->GetArgAsString(
        kArgumentBufferId, std::string() /* default_value */);
    if (buffer_id.empty() || id.empty()) {
      LOG(ERROR) << "Cannot map id to buffer id for event: "
                 << event->ToString();
      continue;
    }
    if (buffer_in_use_id_to_buffer_id.count(id) &&
        buffer_in_use_id_to_buffer_id[id] != buffer_id) {
      LOG(ERROR) << id << " is already mapped to "
                 << buffer_in_use_id_to_buffer_id[id] << ". Skip mapping to "
                 << buffer_id;
      continue;
    }
    buffer_in_use_id_to_buffer_id[id] = buffer_id;
  }

  for (const ArcTracingEvent* event : buffer_in_use_events) {
    if (event->GetPhase() != TRACE_EVENT_PHASE_ASYNC_STEP_INTO)
      continue;
    const std::string id = event->GetId();
    if (!buffer_in_use_id_to_buffer_id.count(id)) {
      LOG(ERROR) << "Found non-mapped event: " << event->ToString();
      continue;
    }
    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        per_buffer_chrome_events[buffer_in_use_id_to_buffer_id[id]];
    GetEventMapper().Produce(*event, &graphics_events);
  }

  for (auto& buffer : per_buffer_chrome_events)
    SortBufferEventsByTimestamp(&buffer.second);

  return per_buffer_chrome_events;
}

void ScanForCustomEvents(
    const ArcTracingEvent* event,
    ArcTracingGraphicsModel::BufferEvents* out_custom_events) {
  if (base::StartsWith(event->GetName(), kCustomTracePrefix,
                       base::CompareCase::SENSITIVE)) {
    DCHECK(!event->GetArgs() || event->GetArgs()->empty());
    out_custom_events->emplace_back(
        ArcTracingGraphicsModel::BufferEventType::kCustomEvent,
        event->GetTimestamp(),
        event->GetName().substr(base::size(kCustomTracePrefix) - 1));
  }
  for (const auto& child : event->children())
    ScanForCustomEvents(child.get(), out_custom_events);
}

// Extracts custom events from the model. Custom events start from customTrace
ArcTracingGraphicsModel::BufferEvents GetCustomEvents(
    const ArcTracingModel& common_model) {
  ArcTracingGraphicsModel::BufferEvents custom_events;
  for (const ArcTracingEvent* root : common_model.GetRoots())
    ScanForCustomEvents(root, &custom_events);
  return custom_events;
}

// Helper that finds a event of particular type in the list of events |events|
// starting from the index |start_index|. Returns |kInvalidBufferIndex| if event
// cannot be found.
ssize_t FindEvent(const ArcTracingGraphicsModel::BufferEvents& events,
                  BufferEventType type,
                  size_t start_index) {
  for (size_t i = start_index; i < events.size(); ++i) {
    if (events[i].type == type)
      return i;
  }
  return kInvalidBufferIndex;
}

// Helper that finds valid pair of events for acquire/release buffer.
// |kBufferQueueReleased| should go immediately after |kBufferQueueAcquire|
// event with one exception of |kBufferQueueDequeueStart| that is allowed due to
// asynchronous flow of requesting buffers in Android. Returns
// |kInvalidBufferIndex| if such pair cannot be found.
ssize_t FindAcquireReleasePair(
    const ArcTracingGraphicsModel::BufferEvents& events,
    size_t start_index) {
  const ssize_t index_acquire =
      FindEvent(events, BufferEventType::kBufferQueueAcquire, start_index);
  if (index_acquire == kInvalidBufferIndex)
    return kInvalidBufferIndex;

  // kBufferQueueDequeueStart is allowed between kBufferQueueAcquire and
  // kBufferQueueReleased.
  for (size_t i = index_acquire + 1; i < events.size(); ++i) {
    if (events[i].type == BufferEventType::kBufferQueueDequeueStart) {
      continue;
    }
    if (events[i].type == BufferEventType::kBufferQueueReleased) {
      return index_acquire;
    }
    break;
  }
  return kInvalidBufferIndex;
}

// Helper that performs bisection search of event of type |type| in the ordered
// list of events |events|. Found event should have timestamp not later than
// |timestamp|. Returns |kInvalidBufferIndex| in case event is not found.
ssize_t FindNotLaterThan(const ArcTracingGraphicsModel::BufferEvents& events,
                         BufferEventType type,
                         uint64_t timestamp) {
  if (events.empty() || events[0].timestamp > timestamp)
    return kInvalidBufferIndex;

  size_t min_range = 0;
  size_t result = events.size() - 1;
  while (events[result].timestamp > timestamp) {
    const size_t next = (result + min_range + 1) / 2;
    if (events[next].timestamp <= timestamp)
      min_range = next;
    else
      result = next - 1;
  }
  for (ssize_t i = result; i >= 0; --i) {
    if (events[i].type == type)
      return i;
  }
  return kInvalidBufferIndex;
}

// Tries to match Android graphics buffer events and Chrome graphics buffer
// events. There is no direct id usable to say if the same buffer is used or
// not. This tests if two set of events potentially belong the same buffer and
// return the maximum number of matching sequences. In case impossible
// combination is found then it returns 0 score. Impossible combination for
// example when we detect Chrome buffer was attached while it was not held by
// Android between |kBufferQueueAcquire| and |kBufferQueueRelease.
// The process of merging buffers continues while we can merge something. At
// each iteration buffers with maximum merge score get merged. Practically,
// having 20+ cycles (assuming 4 buffers in use) is enough to exactly identify
// the same buffer in Chrome and Android. If needed more similar checks can be
// added.
size_t GetMergeScore(
    const ArcTracingGraphicsModel::BufferEvents& surface_flinger_events,
    const ArcTracingGraphicsModel::BufferEvents& chrome_events) {
  ssize_t attach_index = -1;
  ssize_t acquire_index = -1;
  while (true) {
    acquire_index =
        FindAcquireReleasePair(surface_flinger_events, acquire_index + 1);
    if (acquire_index == kInvalidBufferIndex)
      return 0;
    attach_index =
        FindNotLaterThan(chrome_events, BufferEventType::kExoSurfaceAttach,
                         surface_flinger_events[acquire_index + 1].timestamp);
    if (attach_index >= 0)
      break;
  }
  // From here buffers must be in sync. Attach should happen between acquire and
  // release.
  size_t score = 0;
  while (true) {
    const int64_t timestamp_from =
        surface_flinger_events[acquire_index].timestamp;
    const int64_t timestamp_to =
        surface_flinger_events[acquire_index + 1].timestamp;
    const int64_t timestamp = chrome_events[attach_index].timestamp;
    if (timestamp < timestamp_from || timestamp > timestamp_to) {
      return 0;
    }
    acquire_index =
        FindAcquireReleasePair(surface_flinger_events, acquire_index + 2);
    attach_index = FindEvent(chrome_events, BufferEventType::kExoSurfaceAttach,
                             attach_index + 1);
    if (acquire_index == kInvalidBufferIndex ||
        attach_index == kInvalidBufferIndex) {
      break;
    }
    ++score;
  }

  return score;
}

// Adds jank events into |ArcTracingGraphicsModel::EventsContainer|.
// |pulse_event_type| defines the type of the event that should appear
// periodically. Once it is missed in analyzed buffer events, new jank event is
// added. |jank_event_type| defines the type of jank.
void AddJanks(ArcTracingGraphicsModel::EventsContainer* result,
              BufferEventType pulse_event_type,
              BufferEventType jank_event_type) {
  // Detect rate first.
  BufferEvents pulse_events;

  for (const auto& it : result->buffer_events()) {
    for (const auto& it_event : it) {
      if (it_event.type == pulse_event_type)
        pulse_events.emplace_back(it_event);
    }
  }
  SortBufferEventsByTimestamp(&pulse_events);

  ArcGraphicsJankDetector jank_detector(base::BindRepeating(
      [](BufferEventType jank_event_type, BufferEvents* out_janks,
         const base::Time& timestamp) {
        out_janks->emplace_back(
            BufferEvent(jank_event_type,
                        timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds()));
      },
      jank_event_type, &result->global_events()));

  for (const auto& it : pulse_events) {
    jank_detector.OnSample(base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(it.timestamp)));
    if (jank_detector.stage() == ArcGraphicsJankDetector::Stage::kActive)
      break;
  }
  // At this point, no janks should be reported. We are detecting the rate.
  if (jank_detector.stage() != ArcGraphicsJankDetector::Stage::kActive)
    return;

  // Period is defined. Pass all samples to detect janks.
  jank_detector.SetPeriodFixed(jank_detector.period());
  for (const auto& it : pulse_events) {
    jank_detector.OnSample(base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(it.timestamp)));
  }
}

// Helper that performs query in |common_model| for top level Chrome GPU events
// and returns bands of sorted list of built events.
void GetChromeTopLevelEvents(const ArcTracingModel& common_model,
                             ArcTracingGraphicsModel::EventsContainer* result) {
  // There is a chance that Chrome top level events may overlap. This may happen
  // in case on non-trivial GPU load. In this case notification about swap or
  // presentation done may come after the next frame draw is started. As a
  // result, it leads to confusion in case displayed on the same event band.
  // Solution is to allocate extra band and interchange events per buffer.
  // Events are grouped per frame's id that starts from 0x100000000 and has
  // monotonous increment. So we can simple keep it in the tree map that
  // provides us the right ordering.
  std::map<std::string, std::vector<const ArcTracingEvent*>> per_frame_events;

  for (const ArcTracingEvent* event :
       common_model.Select(kChromeTopEventsQuery)) {
    per_frame_events[event->GetId()].emplace_back(event);
  }

  size_t band_index = 0;
  result->buffer_events().resize(2);
  for (const auto& it_frame : per_frame_events) {
    for (const ArcTracingEvent* event : it_frame.second)
      GetEventMapper().Produce(*event, &result->buffer_events()[band_index]);
    band_index = (band_index + 1) % result->buffer_events().size();
  }

  for (auto& chrome_top_level_band : result->buffer_events())
    SortBufferEventsByTimestamp(&chrome_top_level_band);

  AddJanks(result, BufferEventType::kChromeOSDraw,
           BufferEventType::kChromeOSJank);
}

// Helper that extracts top level Android events, such as refresh, vsync.
void GetAndroidTopEvents(const ArcTracingModel& common_model,
                         ArcTracingGraphicsModel::EventsContainer* result) {
  result->buffer_events().resize(1);
  for (const ArcTracingEvent* event :
       common_model.Select(kHandleMessageRefreshQuery)) {
    GetEventMapper().Produce(*event, &result->buffer_events()[0]);
  }
  for (const ArcTracingEvent* event :
       common_model.Select(kHandleMessageInvalidateQuery)) {
    GetEventMapper().Produce(*event, &result->buffer_events()[0]);
  }

  const BufferGraphicsEventMapper::MappingRule
      mapArcTimestampTraceEventToVsyncTimestamp(
          std::make_unique<ArcTracingEventMatcher>(kArcVsyncTimestampQuery),
          BufferEventType::kVsyncTimestamp, BufferEventType::kNone,
          base::BindRepeating([](const ArcTracingEventMatcher& matcher,
                                 const ArcTracingEvent& event) {
            // kVsyncTimestamp is special in that we get the actual/ideal
            // vsync timestamp which is provided as extra metadata encoded in
            // the event name string, rather than looking at the the timestamp
            // at which the event was recorded.
            base::Optional<int64_t> timestamp =
                matcher.ReadAndroidEventInt64(event);

            // The encoded int64 timestamp is in nanoseconds. Convert to
            // microseconds as that is what the event timestamps are in.
            return timestamp ? (*timestamp / 1000) : event.GetTimestamp();
          }));
  bool hasArcVsyncTimestampEvents = false;
  for (const ArcTracingEvent* event :
       common_model.Select(kArcVsyncTimestampQuery)) {
    if (mapArcTimestampTraceEventToVsyncTimestamp.Produce(
            *event, &result->global_events())) {
      hasArcVsyncTimestampEvents = true;
    }
  }

  if (!hasArcVsyncTimestampEvents) {
    // For backwards compatibility, if there are no events that match
    // kArcVsyncTimestampQuery, we use the timestamp of the events that match
    // kSurfaceFlingerVsyncHandlerQuery as the kVsyncTimestamp event, though
    // this does not accurately represent the vsync event timestamp.
    const BufferGraphicsEventMapper::MappingRule
        mapVsyncHandlerTraceToVsyncTimestamp(
            std::make_unique<ArcTracingEventMatcher>(
                kSurfaceFlingerVsyncHandlerQuery),
            BufferEventType::kVsyncTimestamp, BufferEventType::kNone);

    for (const ArcTracingEvent* event :
         common_model.Select(kSurfaceFlingerVsyncHandlerQuery))
      mapVsyncHandlerTraceToVsyncTimestamp.Produce(*event,
                                                   &result->global_events());
  }

  const BufferGraphicsEventMapper::MappingRule
      mapVsyncHandlerTraceToVsyncHandlerEvent(
          std::make_unique<ArcTracingEventMatcher>(
              kSurfaceFlingerVsyncHandlerQuery),
          BufferEventType::kSurfaceFlingerVsyncHandler, BufferEventType::kNone);
  for (const ArcTracingEvent* event :
       common_model.Select(kSurfaceFlingerVsyncHandlerQuery))
    mapVsyncHandlerTraceToVsyncHandlerEvent.Produce(*event,
                                                    &result->global_events());

  SortBufferEventsByTimestamp(&result->buffer_events()[0]);

  AddJanks(result, BufferEventType::kSurfaceFlingerCompositionStart,
           BufferEventType::kSurfaceFlingerCompositionJank);
  SortBufferEventsByTimestamp(&result->global_events());
}

// Helper that serializes events |events| to the |base::ListValue|.
base::ListValue SerializeEvents(
    const ArcTracingGraphicsModel::BufferEvents& events) {
  base::ListValue list;
  for (const auto& event : events) {
    base::ListValue event_value;
    event_value.Append(base::Value(static_cast<int>(event.type)));
    event_value.Append(base::Value(static_cast<double>(event.timestamp)));
    if (!event.content.empty())
      event_value.Append(base::Value(event.content));
    list.Append(std::move(event_value));
  }
  return list;
}

// Helper that serializes |events| to the |base::DictionaryValue|.
base::DictionaryValue SerializeEventsContainer(
    const ArcTracingGraphicsModel::EventsContainer& events) {
  base::DictionaryValue dictionary;

  base::ListValue buffer_list;
  for (auto& buffer : events.buffer_events())
    buffer_list.Append(SerializeEvents(buffer));

  dictionary.SetKey(kKeyBuffers, std::move(buffer_list));
  dictionary.SetKey(kKeyGlobalEvents, SerializeEvents(events.global_events()));

  return dictionary;
}

bool IsInRange(BufferEventType type,
               BufferEventType type_from_inclusive,
               BufferEventType type_to_inclusive) {
  return type >= type_from_inclusive && type <= type_to_inclusive;
}

// Helper that loads events from |base::Value|. Returns true in case events were
// read successfully. Events must be sorted and be known.
bool LoadEvents(const base::Value* value,
                ArcTracingGraphicsModel::BufferEvents* out_events) {
  DCHECK(out_events);
  if (!value || !value->is_list())
    return false;
  int64_t previous_timestamp = 0;
  for (const auto& entry : value->GetList()) {
    if (!entry.is_list() || entry.GetList().size() < 2)
      return false;
    if (!entry.GetList()[0].is_int())
      return false;
    const BufferEventType type =
        static_cast<BufferEventType>(entry.GetList()[0].GetInt());

    if (!IsInRange(type, BufferEventType::kBufferQueueDequeueStart,
                   BufferEventType::kBufferFillJank) &&
        !IsInRange(type, BufferEventType::kExoSurfaceAttach,
                   BufferEventType::kExoSurfaceCommit) &&
        !IsInRange(type, BufferEventType::kChromeBarrierOrder,
                   BufferEventType::kChromeBarrierFlush) &&
        !IsInRange(type, BufferEventType::kSurfaceFlingerVsyncHandler,
                   BufferEventType::kVsyncTimestamp) &&
        !IsInRange(type, BufferEventType::kChromeOSDraw,
                   BufferEventType::kChromeOSJank) &&
        !IsInRange(type, BufferEventType::kCustomEvent,
                   BufferEventType::kCustomEvent) &&
        !IsInRange(type, BufferEventType::kInputEventCreated,
                   BufferEventType::kInputEventDeliverEnd)) {
      return false;
    }

    if (!entry.GetList()[1].is_double() && !entry.GetList()[1].is_int())
      return false;
    const int64_t timestamp = entry.GetList()[1].GetDouble();
    if (timestamp < previous_timestamp)
      return false;
    if (entry.GetList().size() == 3) {
      if (!entry.GetList()[2].is_string())
        return false;
      out_events->emplace_back(type, timestamp, entry.GetList()[2].GetString());
    } else {
      out_events->emplace_back(type, timestamp);
    }
    previous_timestamp = timestamp;
  }
  return true;
}

bool LoadEventsContainer(const base::Value* value,
                         ArcTracingGraphicsModel::EventsContainer* out_events) {
  DCHECK(out_events->buffer_events().empty());
  DCHECK(out_events->global_events().empty());

  if (!value || !value->is_dict())
    return false;

  const base::DictionaryValue* dictionary = nullptr;
  value->GetAsDictionary(&dictionary);
  DCHECK(dictionary);

  const base::Value* buffer_entries =
      dictionary->FindKeyOfType(kKeyBuffers, base::Value::Type::LIST);
  if (!buffer_entries)
    return false;

  for (const auto& buffer_entry : buffer_entries->GetList()) {
    BufferEvents events;
    if (!LoadEvents(&buffer_entry, &events))
      return false;
    out_events->buffer_events().emplace_back(std::move(events));
  }

  const base::Value* const global_events =
      dictionary->FindKeyOfType(kKeyGlobalEvents, base::Value::Type::LIST);
  if (!LoadEvents(global_events, &out_events->global_events()))
    return false;

  return true;
}

bool ReadDuration(const base::Value* root, uint32_t* duration) {
  const base::Value* duration_value = root->FindKey(kKeyDuration);
  if (!duration_value ||
      (!duration_value->is_double() && !duration_value->is_int())) {
    return false;
  }

  *duration = duration_value->GetDouble();
  if (*duration < 0)
    return false;

  return true;
}

}  // namespace

ArcTracingGraphicsModel::BufferEvent::BufferEvent(BufferEventType type,
                                                  int64_t timestamp)
    : type(type), timestamp(timestamp) {}

ArcTracingGraphicsModel::BufferEvent::BufferEvent(BufferEventType type,
                                                  int64_t timestamp,
                                                  const std::string& content)
    : type(type), timestamp(timestamp), content(content) {}

bool ArcTracingGraphicsModel::BufferEvent::operator==(
    const BufferEvent& other) const {
  return type == other.type && timestamp == other.timestamp &&
         content == other.content;
}

ArcTracingGraphicsModel::ViewId::ViewId(int task_id,
                                        const std::string& activity)
    : task_id(task_id), activity(activity) {}

bool ArcTracingGraphicsModel::ViewId::operator<(const ViewId& other) const {
  if (task_id != other.task_id)
    return task_id < other.task_id;
  return activity.compare(other.activity) < 0;
}

bool ArcTracingGraphicsModel::ViewId::operator==(const ViewId& other) const {
  return task_id == other.task_id && activity == other.activity;
}

ArcTracingGraphicsModel::ArcTracingGraphicsModel() = default;

ArcTracingGraphicsModel::~ArcTracingGraphicsModel() = default;

// static
void ArcTracingGraphicsModel::TrimEventsContainer(
    ArcTracingGraphicsModel::EventsContainer* container,
    int64_t trim_timestamp,
    const std::set<ArcTracingGraphicsModel::BufferEventType>& start_types) {
  // For trim point use |ArcTracingGraphicsModel::BufferEventType::kNone| that
  // is less than any other event type.
  const ArcTracingGraphicsModel::BufferEvent trim_point(
      ArcTracingGraphicsModel::BufferEventType::kNone, trim_timestamp);

  // Global events are trimmed by timestamp only.
  auto global_cut_pos = std::lower_bound(container->global_events().begin(),
                                         container->global_events().end(),
                                         trim_point, SortByTimestampPred);
  container->global_events() = ArcTracingGraphicsModel::BufferEvents(
      global_cut_pos, container->global_events().end());

  for (auto& buffer : container->buffer_events()) {
    auto cut_pos = std::lower_bound(buffer.begin(), buffer.end(), trim_point,
                                    SortByTimestampPred);
    while (cut_pos != buffer.end()) {
      if (start_types.count(cut_pos->type))
        break;
      ++cut_pos;
    }

    buffer = ArcTracingGraphicsModel::BufferEvents(cut_pos, buffer.end());
  }
}

bool ArcTracingGraphicsModel::Build(const ArcTracingModel& common_model) {
  Reset();

  BufferToEvents per_buffer_surface_flinger_events;
  if (!GetSurfaceFlingerEvents(common_model,
                               &per_buffer_surface_flinger_events)) {
    if (!skip_structure_validation_)
      return false;
  }
  BufferToEvents per_buffer_chrome_events =
      GetChromeEvents(common_model, &chrome_buffer_id_to_task_id_);

  // Try to merge surface flinger events and Chrome events. See |GetMergeScore|
  // for more details.
  while (true) {
    size_t max_merge_score = 0;
    std::string surface_flinger_buffer_id;
    std::string chrome_buffer_id;
    for (const auto& surface_flinger_buffer :
         per_buffer_surface_flinger_events) {
      for (const auto& chrome_buffer : per_buffer_chrome_events) {
        const size_t merge_score =
            GetMergeScore(surface_flinger_buffer.second, chrome_buffer.second);
        if (merge_score > max_merge_score) {
          max_merge_score = merge_score;
          surface_flinger_buffer_id = surface_flinger_buffer.first;
          chrome_buffer_id = chrome_buffer.first;
        }
      }
    }
    if (!max_merge_score)
      break;  // No more merge candidates.

    const ViewId view_id(GetTaskIdFromBufferName(chrome_buffer_id),
                         GetActivityFromBufferName(surface_flinger_buffer_id));

    std::vector<BufferEvents>& view_buffers =
        view_buffers_[view_id].buffer_events();
    view_buffers.push_back(std::move(
        per_buffer_surface_flinger_events[surface_flinger_buffer_id]));
    per_buffer_surface_flinger_events.erase(surface_flinger_buffer_id);
    view_buffers.back().insert(
        view_buffers.back().end(),
        per_buffer_chrome_events[chrome_buffer_id].begin(),
        per_buffer_chrome_events[chrome_buffer_id].end());
    per_buffer_chrome_events.erase(chrome_buffer_id);
    SortBufferEventsByTimestamp(&view_buffers.back());
  }

  for (auto& buffer : per_buffer_surface_flinger_events) {
    LOG(WARNING) << "Failed to merge events for buffer: " << buffer.first;
    view_buffers_[ViewId(-1 /* task_id */,
                         GetActivityFromBufferName(buffer.first))]
        .buffer_events()
        .emplace_back(std::move(buffer.second));
  }

  for (auto& buffer : per_buffer_chrome_events) {
    LOG(WARNING) << "Failed to merge events for buffer: " << buffer.first;
    view_buffers_[ViewId(GetTaskIdFromBufferName(buffer.first),
                         kUnknownActivity)]
        .buffer_events()
        .emplace_back(std::move(buffer.second));
  }

  if (view_buffers_.empty()) {
    LOG(ERROR) << "No buffer events";
    if (!skip_structure_validation_)
      return false;
  }

  // TODO(khmel): Add more information to resolve owner of custom events. At
  // this moment add custom events to each view.
  const ArcTracingGraphicsModel::BufferEvents custom_events =
      GetCustomEvents(common_model);
  for (auto& it : view_buffers_) {
    AddJanks(&it.second, BufferEventType::kBufferQueueDequeueStart,
             BufferEventType::kBufferFillJank);
    AddJanks(&it.second, BufferEventType::kExoSurfaceAttach,
             BufferEventType::kExoJank);
    it.second.global_events().insert(it.second.global_events().end(),
                                     custom_events.begin(),
                                     custom_events.end());
    SortBufferEventsByTimestamp(&it.second.global_events());
  }

  GetChromeTopLevelEvents(common_model, &chrome_top_level_);
  if (chrome_top_level_.buffer_events().empty()) {
    LOG(ERROR) << "No Chrome top events";
    if (!skip_structure_validation_)
      return false;
  }

  GetAndroidTopEvents(common_model, &android_top_level_);
  if (android_top_level_.buffer_events().empty()) {
    LOG(ERROR) << "No Android events";
    if (!skip_structure_validation_)
      return false;
  }

  GetInputEvents(common_model, &input_);

  system_model_.CopyFrom(common_model.system_model());

  VsyncTrim();

  NormalizeTimestamps();

  system_model_.CloseRangeForValueEvents(duration_ - 1 /* max_timestamp */);

  return true;
}

void ArcTracingGraphicsModel::NormalizeTimestamps() {
  std::vector<BufferEvents*> all_buffers;
  for (auto& view : view_buffers_) {
    for (auto& buffer : view.second.buffer_events())
      all_buffers.emplace_back(&buffer);
    all_buffers.emplace_back(&view.second.global_events());
  }

  std::vector<EventsContainer*> containers{&android_top_level_,
                                           &chrome_top_level_, &input_};
  for (EventsContainer* container : containers) {
    for (auto& buffer : container->buffer_events())
      all_buffers.emplace_back(&buffer);
    all_buffers.emplace_back(&container->global_events());
  }

  uint64_t min = std::numeric_limits<uint64_t>::max();
  uint64_t max = std::numeric_limits<uint64_t>::min();
  for (const BufferEvents* buffer : all_buffers) {
    if (!buffer->empty()) {
      min = std::min(min, buffer->front().timestamp);
      max = std::max(max, buffer->back().timestamp);
    }
  }

  for (const auto& cpu_events : system_model_.all_cpu_events()) {
    if (!cpu_events.empty()) {
      min = std::min(min, cpu_events.front().timestamp);
      max = std::max(max, cpu_events.back().timestamp);
    }
  }

  if (!system_model_.memory_events().empty()) {
    min = std::min(min, system_model_.memory_events().front().timestamp);
    max = std::max(max, system_model_.memory_events().back().timestamp);
  }

  duration_ = max - min + 1;

  for (BufferEvents* buffer : all_buffers) {
    for (auto& event : *buffer)
      event.timestamp -= min;
  }

  for (auto& cpu_events : system_model_.all_cpu_events()) {
    for (auto& cpu_event : cpu_events)
      cpu_event.timestamp -= min;
  }

  for (auto& memory_event : system_model_.memory_events())
    memory_event.timestamp -= min;
}

void ArcTracingGraphicsModel::Reset() {
  chrome_top_level_.Reset();
  android_top_level_.Reset();
  input_.Reset();
  view_buffers_.clear();
  chrome_buffer_id_to_task_id_.clear();
  system_model_.Reset();
  duration_ = 0;
  app_title_ = std::string();
  app_icon_png_.clear();
  platform_ = std::string();
  timestamp_ = base::Time();
}

void ArcTracingGraphicsModel::VsyncTrim() {
  int64_t trim_timestamp = -1;

  for (const auto& it : android_top_level_.global_events()) {
    if (it.type == BufferEventType::kVsyncTimestamp) {
      trim_timestamp = it.timestamp;
      break;
    }
  }

  if (trim_timestamp < 0) {
    LOG(ERROR) << "VSYNC event was not found, could not trim.";
    return;
  }

  TrimEventsContainer(&chrome_top_level_, trim_timestamp,
                      {BufferEventType::kChromeOSDraw});
  TrimEventsContainer(&android_top_level_, trim_timestamp,
                      {BufferEventType::kSurfaceFlingerInvalidationStart,
                       BufferEventType::kSurfaceFlingerCompositionStart});
  TrimEventsContainer(&input_, trim_timestamp,
                      {BufferEventType::kInputEventCreated});
  for (auto& view_buffer : view_buffers_) {
    TrimEventsContainer(&view_buffer.second, trim_timestamp,
                        {BufferEventType::kBufferQueueDequeueStart,
                         BufferEventType::kExoSurfaceAttach,
                         BufferEventType::kChromeBarrierOrder});
  }
  system_model_.Trim(trim_timestamp);
}

int ArcTracingGraphicsModel::GetTaskIdFromBufferName(
    const std::string& chrome_buffer_name) const {
  const auto it = chrome_buffer_id_to_task_id_.find(chrome_buffer_name);
  if (it == chrome_buffer_id_to_task_id_.end())
    return -1;
  return it->second;
}

std::unique_ptr<base::DictionaryValue> ArcTracingGraphicsModel::Serialize()
    const {
  std::unique_ptr<base::DictionaryValue> root =
      std::make_unique<base::DictionaryValue>();

  // Views
  base::ListValue view_list;
  for (auto& view : view_buffers_) {
    base::DictionaryValue view_value = SerializeEventsContainer(view.second);
    view_value.SetKey(kKeyActivity, base::Value(view.first.activity));
    view_value.SetKey(kKeyTaskId, base::Value(view.first.task_id));
    view_list.Append(std::move(view_value));
  }
  root->SetKey(kKeyViews, std::move(view_list));

  // Android top events.
  root->SetKey(kKeyAndroid, SerializeEventsContainer(android_top_level_));

  // Chrome top events
  root->SetKey(kKeyChrome, SerializeEventsContainer(chrome_top_level_));

  // Input events
  root->SetKey(kKeyInput, SerializeEventsContainer(input_));

  // System.
  root->SetKey(kKeySystem, system_model_.Serialize());

  // Information
  base::DictionaryValue information;
  information.SetKey(kKeyDuration, base::Value(static_cast<double>(duration_)));
  if (!platform_.empty())
    information.SetKey(kKeyPlatform, base::Value(platform_));
  if (!timestamp_.is_null())
    information.SetKey(kKeyTimestamp, base::Value(timestamp_.ToJsTime()));
  if (!app_title_.empty())
    information.SetKey(kKeyTitle, base::Value(app_title_));
  if (!app_icon_png_.empty()) {
    const std::string png_data_as_string(
        reinterpret_cast<const char*>(&app_icon_png_[0]), app_icon_png_.size());
    std::string icon_content;
    base::Base64Encode(png_data_as_string, &icon_content);
    information.SetKey(kKeyIcon, base::Value(icon_content));
  }
  root->SetKey(kKeyInformation, std::move(information));

  return root;
}

std::string ArcTracingGraphicsModel::SerializeToJson() const {
  std::unique_ptr<base::DictionaryValue> root = Serialize();
  DCHECK(root);
  std::string output;
  if (!base::JSONWriter::WriteWithOptions(
          *root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output)) {
    LOG(ERROR) << "Failed to serialize model";
  }
  return output;
}

bool ArcTracingGraphicsModel::LoadFromJson(const std::string& json_data) {
  Reset();
  const std::unique_ptr<base::DictionaryValue> root =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(json_data));
  if (!root)
    return false;
  return LoadFromValue(*root);
}

bool ArcTracingGraphicsModel::LoadFromValue(const base::DictionaryValue& root) {
  Reset();

  const base::Value* view_list =
      root.FindKeyOfType(kKeyViews, base::Value::Type::LIST);
  if (!view_list || view_list->GetList().empty())
    return false;

  for (const auto& view_entry : view_list->GetList()) {
    if (!view_entry.is_dict())
      return false;
    const base::Value* activity =
        view_entry.FindKeyOfType(kKeyActivity, base::Value::Type::STRING);
    const base::Value* task_id =
        view_entry.FindKeyOfType(kKeyTaskId, base::Value::Type::INTEGER);
    if (!activity || !task_id)
      return false;
    const ViewId view_id(task_id->GetInt(), activity->GetString());
    if (view_buffers_.find(view_id) != view_buffers_.end())
      return false;

    if (!LoadEventsContainer(&view_entry, &view_buffers_[view_id]))
      return false;
  }

  if (!LoadEventsContainer(root.FindKey(kKeyAndroid), &android_top_level_))
    return false;

  if (!LoadEventsContainer(root.FindKey(kKeyChrome), &chrome_top_level_))
    return false;

  const base::Value* input_value = root.FindKey(kKeyInput);
  if (input_value && !LoadEventsContainer(input_value, &input_))
    return false;

  if (!system_model_.Load(root.FindKey(kKeySystem)))
    return false;

  const base::Value* informaton =
      root.FindKeyOfType(kKeyInformation, base::Value::Type::DICTIONARY);
  if (informaton) {
    if (!ReadDuration(informaton, &duration_))
      return false;

    const base::Value* platform_value =
        informaton->FindKeyOfType(kKeyPlatform, base::Value::Type::STRING);
    if (platform_value)
      platform_ = platform_value->GetString();
    const base::Value* title_value =
        informaton->FindKeyOfType(kKeyTitle, base::Value::Type::STRING);
    if (title_value)
      app_title_ = title_value->GetString();
    const base::Value* icon_value =
        informaton->FindKeyOfType(kKeyIcon, base::Value::Type::STRING);
    if (icon_value) {
      std::string icon_content;
      if (!base::Base64Decode(icon_value->GetString(), &icon_content))
        return false;
      app_icon_png_ =
          std::vector<unsigned char>(icon_content.begin(), icon_content.end());
    }
    const base::Value* timestamp_value =
        informaton->FindKeyOfType(kKeyTimestamp, base::Value::Type::DOUBLE);
    if (timestamp_value)
      timestamp_ = base::Time::FromJsTime(timestamp_value->GetDouble());
  } else {
    if (!ReadDuration(&root, &duration_))
      return false;
  }

  return true;
}

ArcTracingGraphicsModel::EventsContainer::EventsContainer() = default;

ArcTracingGraphicsModel::EventsContainer::~EventsContainer() = default;

void ArcTracingGraphicsModel::EventsContainer::Reset() {
  buffer_events_.clear();
  global_events_.clear();
}

bool ArcTracingGraphicsModel::EventsContainer::operator==(
    const EventsContainer& other) const {
  return buffer_events() == other.buffer_events() &&
         global_events() == other.global_events();
}

std::ostream& operator<<(std::ostream& os,
                         ArcTracingGraphicsModel::BufferEventType event_type) {
  return os << static_cast<typename std::underlying_type<
             ArcTracingGraphicsModel::BufferEventType>::type>(event_type);
}

}  // namespace arc
