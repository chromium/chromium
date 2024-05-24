// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"

#include <inttypes.h>

#include <algorithm>
#include <set>

#include "ash/components/arc/arc_util.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/arc/tracing/arc_graphics_jank_detector.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_event.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_event_matcher.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_model.h"
#include "chrome/browser/ash/arc/tracing/present_frames_tracer.h"

namespace arc {

namespace {

using BufferEvent = ArcTracingGraphicsModel::BufferEvent;
using BufferEvents = ArcTracingGraphicsModel::BufferEvents;
using EventType = ArcTracingGraphicsModel::EventType;

constexpr char kUnknownActivity[] = "unknown";

constexpr char kKeyActivity[] = "activity";
constexpr char kKeyBuffers[] = "buffers";
constexpr char kKeyChrome[] = "chrome";
constexpr char kKeyAppFps[] = "app_fps";
constexpr char kKeyGlobalEvents[] = "global_events";
constexpr char kKeyIcon[] = "icon";
constexpr char kKeyViews[] = "views";
constexpr char kKeyPlatform[] = "platform";
constexpr char kKeySystem[] = "system";
constexpr char kKeyTaskId[] = "task_id";
constexpr char kKeyTimestamp[] = "timestamp";
constexpr char kKeyTitle[] = "title";

constexpr char kDequeueBufferQuery[] = "android:dequeueBuffer";
constexpr char kQueueBufferQuery[] = "android:queueBuffer";

constexpr char kFrameDisplayedQuery[] =
    "benchmark,viz," TRACE_DISABLED_BY_DEFAULT(
        "display.framedisplayed") ":Display::FrameDisplayed";

// Helper factory class that produces graphic buffer events from the giving
// |ArcTracingEvent| generic events. Each |ArcTracingEvent| may produce graphics
// event |ArcTracingGraphicsModel::BufferEvent| on start or/and on finish of the
// event |ArcTracingEvent|. This is organized in form of map
// |ArcTracingEventMatcher| to the pair of |EventType| which indicates
// what to generate on start and on finish of the event.
class BufferGraphicsEventMapper {
 public:
  struct MappingRule {
    using EventTimeCallback =
        base::RepeatingCallback<uint64_t(const ArcTracingEventMatcher&,
                                         const ArcTracingEvent& event)>;

    MappingRule(std::unique_ptr<ArcTracingEventMatcher> matcher,
                EventType map_start,
                EventType map_finish,
                EventTimeCallback start_time_callback = EventTimeCallback())
        : matcher(std::move(matcher)),
          map_start(map_start),
          map_finish(map_finish),
          event_start_time_callback(start_time_callback) {}

    bool Produce(const ArcTracingEvent& event,
                 ArcTracingGraphicsModel::BufferEvents* collector) const {
      if (!matcher->Match(event)) {
        return false;
      }

      if (map_start != EventType::kNone) {
        uint64_t start_timestamp = event.GetTimestamp();
        if (event_start_time_callback) {
          start_timestamp = event_start_time_callback.Run(*matcher, event);
        }

        collector->emplace_back(map_start, start_timestamp);
      }
      if (map_finish != EventType::kNone) {
        collector->emplace_back(map_finish, event.GetEndTimestamp());
      }

      return true;
    }

    std::unique_ptr<ArcTracingEventMatcher> matcher;
    EventType map_start;
    EventType map_finish;
    EventTimeCallback event_start_time_callback;
  };
  using MappingRules = std::vector<MappingRule>;

  BufferGraphicsEventMapper() {
    // android rules
    rules_.emplace_back(
        std::make_unique<ArcTracingEventMatcher>(kDequeueBufferQuery),
        EventType::kBufferQueueDequeueStart,
        EventType::kBufferQueueDequeueDone);
    rules_.emplace_back(
        std::make_unique<ArcTracingEventMatcher>(kQueueBufferQuery),
        EventType::kBufferQueueQueueStart, EventType::kBufferQueueQueueDone);
    rules_.emplace_back(
        std::make_unique<ArcTracingEventMatcher>("android:acquireBuffer"),
        EventType::kBufferQueueAcquire, EventType::kNone);
    rules_.emplace_back(
        std::make_unique<ArcTracingEventMatcher>("android:releaseBuffer"),
        EventType::kNone, EventType::kBufferQueueReleased);
    rules_.emplace_back(std::make_unique<ArcTracingEventMatcher>(
                            "android:handleMessageInvalidate"),
                        EventType::kSurfaceFlingerInvalidationStart,
                        EventType::kSurfaceFlingerInvalidationDone);
    rules_.emplace_back(std::make_unique<ArcTracingEventMatcher>(
                            "android:handleMessageRefresh"),
                        EventType::kSurfaceFlingerCompositionStart,
                        EventType::kSurfaceFlingerCompositionDone);
  }

  BufferGraphicsEventMapper(const BufferGraphicsEventMapper&) = delete;
  BufferGraphicsEventMapper& operator=(const BufferGraphicsEventMapper&) =
      delete;

  ~BufferGraphicsEventMapper() = default;

  void Produce(const ArcTracingEvent& event,
               ArcTracingGraphicsModel::BufferEvents* collector) const {
    for (const auto& rule : rules_) {
      if (rule.Produce(event, collector)) {
        return;
      }
    }
    LOG(ERROR) << "Unsupported event: " << event.ToString();
  }

 private:
  MappingRules rules_;
};

// Maps particular buffer to its events.
using BufferToEvents =
    std::map<std::string, ArcTracingGraphicsModel::BufferEvents>;

bool SortByTimestampPred(const ArcTracingGraphicsModel::BufferEvent& a,
                         const ArcTracingGraphicsModel::BufferEvent& b) {
  if (a.timestamp != b.timestamp) {
    return a.timestamp < b.timestamp;
  }
  return static_cast<int>(a.type) < static_cast<int>(b.type);
}

void SortBufferEventsByTimestamp(BufferEvents* events) {
  std::sort(events->begin(), events->end(), SortByTimestampPred);
}

std::string RouteToSelector(const std::vector<const ArcTracingEvent*>& route) {
  std::string result;
  for (const ArcTracingEvent* segment : route) {
    result = result + "/" + segment->GetCategory() + ":" + segment->GetName();
  }
  return result;
}

void DetermineHierarchy(std::vector<const ArcTracingEvent*>* route,
                        const ArcTracingEvent* event,
                        const ArcTracingEventMatcher& matcher,
                        std::string* out_query) {
  if (!out_query->empty()) {
    return;
  }

  route->emplace_back(event);

  if (matcher.Match(*event)) {
    *out_query = RouteToSelector(*route);
  } else {
    for (const auto& child : event->children()) {
      DetermineHierarchy(route, child.get(), matcher, out_query);
    }
  }

  route->pop_back();
}

// Adds jank events into |ArcTracingGraphicsModel::EventsContainer|.
// |pulse_event_type| defines the type of the event that should appear
// periodically. Once it is missed in analyzed buffer events, new jank event is
// added. |jank_event_type| defines the type of jank.
void AddJanks(std::vector<BufferEvent>* events,
              EventType pulse_event_type,
              EventType jank_event_type) {
  // Detect rate first.
  std::vector<base::Time> pulse_events;

  for (const auto& ev : *events) {
    if (ev.type == pulse_event_type) {
      pulse_events.emplace_back(base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(ev.timestamp)));
    }
  }
  if (!ArcGraphicsJankDetector::IsEnoughSamplesToDetect(pulse_events.size())) {
    LOG(WARNING) << "Not enough samples to detect jank: "
                 << pulse_events.size();
    return;
  }
  std::sort(pulse_events.begin(), pulse_events.end());

  ArcGraphicsJankDetector jank_detector(base::BindRepeating(
      [](EventType jank_event_type, BufferEvents* out_janks,
         base::Time timestamp) {
        out_janks->emplace_back(
            jank_event_type,
            timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
      },
      jank_event_type, events));

  for (const auto& it : pulse_events) {
    jank_detector.OnSample(it);
    if (jank_detector.stage() == ArcGraphicsJankDetector::Stage::kActive) {
      break;
    }
  }
  // At this point, no janks should be reported. We are detecting the rate.
  if (jank_detector.stage() != ArcGraphicsJankDetector::Stage::kActive) {
    LOG(ERROR) << "Jank detector was not able to determine rate";
    return;
  }

  // Period is defined. Pass all samples to detect janks.
  jank_detector.SetPeriodFixed(jank_detector.period());
  for (const auto& it : pulse_events) {
    jank_detector.OnSample(it);
  }

  SortBufferEventsByTimestamp(events);
}

// Helper that performs query in |common_model| for top level Chrome GPU events
// and returns bands of sorted list of built events.
void GetChromeTopLevelEvents(const ArcTracingModel& common_model,
                             ArcTracingGraphicsModel::EventsContainer* result) {
  result->buffer_events().resize(1);
  for (const ArcTracingEvent* event :
       common_model.Select(kFrameDisplayedQuery)) {
    result->buffer_events()[0].emplace_back(EventType::kChromeOSSwapDone,
                                            event->GetTimestamp());
  }

  SortBufferEventsByTimestamp(&result->buffer_events()[0]);
}

// Helper that serializes events |events| to the |base::Value::List|.
base::Value::List SerializeEvents(
    const ArcTracingGraphicsModel::BufferEvents& events) {
  base::Value::List list;
  for (const auto& event : events) {
    base::Value::List event_value;
    event_value.Append(static_cast<int>(event.type));
    event_value.Append(static_cast<double>(event.timestamp));
    if (!event.content.empty()) {
      event_value.Append(base::Value(event.content));
    }
    list.Append(std::move(event_value));
  }
  return list;
}

// Helper that serializes |events| to the |base::Value::Dict|.
base::Value::Dict SerializeEventsContainer(
    const ArcTracingGraphicsModel::EventsContainer& events) {
  base::Value::Dict dictionary;

  base::Value::List buffer_list;
  for (auto& buffer : events.buffer_events()) {
    buffer_list.Append(SerializeEvents(buffer));
  }

  dictionary.Set(kKeyBuffers, std::move(buffer_list));
  dictionary.Set(kKeyGlobalEvents, SerializeEvents(events.global_events()));

  return dictionary;
}

bool IsInRange(EventType type,
               EventType type_from_inclusive,
               EventType type_to_inclusive) {
  return type >= type_from_inclusive && type <= type_to_inclusive;
}

// Helper that loads events from |base::Value|. Returns true in case events were
// read successfully. Events must be sorted and be known.
bool LoadEvents(const base::Value::List* value,
                ArcTracingGraphicsModel::BufferEvents* out_events) {
  DCHECK(out_events);
  if (!value) {
    return false;
  }
  int64_t previous_timestamp = 0;
  for (const auto& item : *value) {
    if (!item.is_list()) {
      return false;
    }
    const base::Value::List& entry = item.GetList();
    if (entry.size() < 2) {
      return false;
    }
    if (!entry[0].is_int()) {
      return false;
    }
    const EventType type = static_cast<EventType>(entry[0].GetInt());

    if (!IsInRange(type, EventType::kBufferQueueDequeueStart,
                   EventType::kBufferFillJank) &&
        !IsInRange(type, EventType::kExoSurfaceAttach,
                   EventType::kExoLastEvent) &&
        !IsInRange(type, EventType::kChromeBarrierOrder,
                   EventType::kChromeBarrierFlush) &&
        !IsInRange(type, EventType::kSurfaceFlingerVsyncHandler,
                   EventType::kVsyncTimestamp) &&
        !IsInRange(type, EventType::kChromeOSDraw,
                   EventType::kChromeOSLastEvent) &&
        !IsInRange(type, EventType::kCustomEvent, EventType::kCustomEvent) &&
        !IsInRange(type, EventType::kInputEventCreated,
                   EventType::kInputEventDeliverEnd)) {
      return false;
    }

    if (!entry[1].is_double() && !entry[1].is_int()) {
      return false;
    }
    const int64_t timestamp = entry[1].GetDouble();
    if (timestamp < previous_timestamp) {
      return false;
    }
    if (entry.size() == 3) {
      if (!entry[2].is_string()) {
        return false;
      }
      out_events->emplace_back(type, timestamp, entry[2].GetString());
    } else {
      out_events->emplace_back(type, timestamp);
    }
    previous_timestamp = timestamp;
  }
  return true;
}

bool LoadEventsContainer(const base::Value::Dict* dict,
                         ArcTracingGraphicsModel::EventsContainer* out_events) {
  DCHECK(out_events->buffer_events().empty());
  DCHECK(out_events->global_events().empty());

  if (!dict) {
    return false;
  }

  const base::Value::List* buffer_entries = dict->FindList(kKeyBuffers);
  if (!buffer_entries) {
    return false;
  }

  for (const auto& buffer_entry : *buffer_entries) {
    BufferEvents events;
    if (!LoadEvents(buffer_entry.GetIfList(), &events)) {
      return false;
    }
    out_events->buffer_events().emplace_back(std::move(events));
  }

  const base::Value::List* const global_events =
      dict->FindList(kKeyGlobalEvents);
  if (!LoadEvents(global_events, &out_events->global_events())) {
    return false;
  }

  return true;
}

bool ReadDuration(const base::Value::Dict* root, uint32_t* duration) {
  const base::Value* duration_value = root->Find(kKeyDuration);
  if (!duration_value ||
      (!duration_value->is_double() && !duration_value->is_int())) {
    return false;
  }

  *duration = duration_value->GetDouble();
  if (*duration < 0) {
    return false;
  }

  return true;
}

}  // namespace

ArcTracingGraphicsModel::BufferEvent::BufferEvent(EventType type,
                                                  int64_t timestamp)
    : type(type), timestamp(timestamp) {}

ArcTracingGraphicsModel::BufferEvent::BufferEvent(EventType type,
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
  if (task_id != other.task_id) {
    return task_id < other.task_id;
  }
  return activity.compare(other.activity) < 0;
}

bool ArcTracingGraphicsModel::ViewId::operator==(const ViewId& other) const {
  return task_id == other.task_id && activity == other.activity;
}

ArcTracingGraphicsModel::ArcTracingGraphicsModel() = default;

ArcTracingGraphicsModel::~ArcTracingGraphicsModel() = default;

bool ArcTracingGraphicsModel::Build(const ArcTracingModel& common_model,
                                    const PresentFramesTracer& present_frames) {
  Reset();

  // TODO(b/296595454): Remove the mapping mechanism as it was only needed
  // for arc-graphics-tracing, and use callbacks to only get buffer updates
  // for a single task.
  // Note that JS code for arc-overview-tracing conflates the buffers for all
  // view IDs when calculating app commit time and FPS (see
  // getAppCommitEvents), so we don't gain anything by generating unique view
  // IDs here.
  auto& buffer_events =
      view_buffers_[ViewId(1 /* task_id */, kUnknownActivity)].buffer_events();
  buffer_events.emplace_back();
  for (int64_t ticks : present_frames.commits()) {
    buffer_events[0].emplace_back(EventType::kExoSurfaceCommit, ticks);
  }
  AddJanks(&buffer_events[0], EventType::kExoSurfaceCommit,
           EventType::kExoSurfaceCommitJank);

  for (int64_t ticks : present_frames.presents()) {
    chrome_top_level_.global_events().emplace_back(
        EventType::kChromeOSPresentationDone, ticks);
  }
  AddJanks(&chrome_top_level_.global_events(),
           EventType::kChromeOSPresentationDone,
           EventType::kChromeOSPerceivedJank);

  GetChromeTopLevelEvents(common_model, &chrome_top_level_);
  if (chrome_top_level_.buffer_events().empty()) {
    LOG(ERROR) << "No Chrome top events";
    if (!skip_structure_validation_) {
      return false;
    }
  }
  AddJanks(&chrome_top_level_.buffer_events()[0], EventType::kChromeOSSwapDone,
           EventType::kChromeOSSwapJank);

  system_model_.CopyFrom(common_model.system_model());

  NormalizeTimestamps();

  perceived_fps_ = present_frames.presents().size() * 1'000'000.0 / duration_;
  app_fps_ = present_frames.commits().size() * 1'000'000.0 / duration_;

  system_model_.CloseRangeForValueEvents(duration_ - 1 /* max_timestamp */);

  return true;
}

void ArcTracingGraphicsModel::NormalizeTimestamps() {
  std::vector<BufferEvents*> all_buffers;
  for (auto& view : view_buffers_) {
    for (auto& buffer : view.second.buffer_events()) {
      all_buffers.emplace_back(&buffer);
    }
    all_buffers.emplace_back(&view.second.global_events());
  }

  for (auto& buffer : chrome_top_level_.buffer_events()) {
    all_buffers.emplace_back(&buffer);
  }
  all_buffers.emplace_back(&chrome_top_level_.global_events());

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
    for (auto& event : *buffer) {
      event.timestamp -= min;
    }
  }

  for (auto& cpu_events : system_model_.all_cpu_events()) {
    for (auto& cpu_event : cpu_events) {
      cpu_event.timestamp -= min;
    }
  }

  for (auto& memory_event : system_model_.memory_events()) {
    memory_event.timestamp -= min;
  }
}

void ArcTracingGraphicsModel::Reset() {
  chrome_top_level_.Reset();
  view_buffers_.clear();
  system_model_.Reset();
  duration_ = 0;
  app_fps_ = 0;
  perceived_fps_ = 0;
  app_title_ = std::string();
  app_icon_png_.clear();
  platform_ = std::string();
  timestamp_ = base::Time();
}

base::Value::Dict ArcTracingGraphicsModel::Serialize() const {
  base::Value::Dict root;

  // Views
  base::Value::List view_list;
  for (auto& view : view_buffers_) {
    base::Value::Dict view_value = SerializeEventsContainer(view.second);
    view_value.Set(kKeyActivity, view.first.activity);
    view_value.Set(kKeyTaskId, view.first.task_id);
    view_list.Append(std::move(view_value));
  }
  root.Set(kKeyViews, std::move(view_list));

  // Chrome top events
  root.Set(kKeyChrome, SerializeEventsContainer(chrome_top_level_));

  // System.
  root.Set(kKeySystem, system_model_.Serialize());

  // Information
  base::Value::Dict information;
  information.Set(kKeyDuration, static_cast<double>(duration_));
  information.Set(kKeyPerceivedFps, perceived_fps_);
  information.Set(kKeyAppFps, app_fps_);
  if (!platform_.empty()) {
    information.Set(kKeyPlatform, platform_);
  }
  if (!timestamp_.is_null()) {
    information.Set(kKeyTimestamp, timestamp_.InMillisecondsFSinceUnixEpoch());
  }
  if (!app_title_.empty()) {
    information.Set(kKeyTitle, app_title_);
  }
  if (!app_icon_png_.empty()) {
    const std::string png_data_as_string(
        reinterpret_cast<const char*>(&app_icon_png_[0]), app_icon_png_.size());
    information.Set(kKeyIcon, base::Base64Encode(png_data_as_string));
  }
  root.Set(kKeyInformation, std::move(information));

  return root;
}

std::string ArcTracingGraphicsModel::SerializeToJson() const {
  base::Value::Dict root = Serialize();
  std::string output;
  if (!base::JSONWriter::WriteWithOptions(
          root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output)) {
    LOG(ERROR) << "Failed to serialize model";
  }
  return output;
}

bool ArcTracingGraphicsModel::LoadFromJson(const std::string& json_data) {
  Reset();
  std::optional<base::Value> root = base::JSONReader::Read(json_data);
  if (!root || !root->is_dict()) {
    return false;
  }
  return LoadFromValue(root->GetDict());
}

bool ArcTracingGraphicsModel::LoadFromValue(const base::Value::Dict& root) {
  Reset();

  const base::Value::List* view_list = root.FindList(kKeyViews);
  if (!view_list || view_list->empty()) {
    // Views are optional for overview tracing.
    if (!skip_structure_validation_) {
      return false;
    }
  } else {
    for (const auto& item : *view_list) {
      const base::Value::Dict* view_entry = item.GetIfDict();
      if (!view_entry) {
        return false;
      }
      const std::string* activity = view_entry->FindString(kKeyActivity);
      std::optional<int> task_id = view_entry->FindInt(kKeyTaskId);
      if (!activity || !task_id) {
        return false;
      }
      const ViewId view_id(*task_id, *activity);
      if (view_buffers_.find(view_id) != view_buffers_.end()) {
        return false;
      }

      if (!LoadEventsContainer(view_entry, &view_buffers_[view_id])) {
        return false;
      }
    }
  }

  if (!LoadEventsContainer(root.FindDict(kKeyChrome), &chrome_top_level_)) {
    return false;
  }

  if (!system_model_.Load(root.Find(kKeySystem))) {
    return false;
  }

  const base::Value::Dict* informaton = root.FindDict(kKeyInformation);
  if (informaton) {
    if (!ReadDuration(informaton, &duration_)) {
      return false;
    }

    const auto perceived_fps_value = informaton->FindDouble(kKeyPerceivedFps);
    if (perceived_fps_value.has_value()) {
      perceived_fps_ = *perceived_fps_value;
    }
    const auto app_fps_value = informaton->FindDouble(kKeyAppFps);
    if (app_fps_value.has_value()) {
      app_fps_ = *perceived_fps_value;
    }

    const std::string* platform_value = informaton->FindString(kKeyPlatform);
    if (platform_value) {
      platform_ = *platform_value;
    }
    const std::string* title_value = informaton->FindString(kKeyTitle);
    if (title_value) {
      app_title_ = *title_value;
    }
    const std::string* icon_value = informaton->FindString(kKeyIcon);
    if (icon_value) {
      std::string icon_content;
      if (!base::Base64Decode(*icon_value, &icon_content)) {
        return false;
      }
      app_icon_png_ =
          std::vector<unsigned char>(icon_content.begin(), icon_content.end());
    }
    std::optional<double> timestamp_value =
        informaton->FindDouble(kKeyTimestamp);
    if (timestamp_value) {
      timestamp_ = base::Time::FromMillisecondsSinceUnixEpoch(*timestamp_value);
    }
  } else {
    if (!ReadDuration(&root, &duration_)) {
      return false;
    }
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
                         ArcTracingGraphicsModel::EventType event_type) {
  return os << base::to_underlying(event_type);
}

}  // namespace arc
