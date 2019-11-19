// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_GRAPHICS_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_GRAPHICS_MODEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/tracing/arc_system_model.h"

namespace arc {

class ArcTracingModel;

// Graphic buffers events model. It is build from the generic |ArcTracingModel|
// and contains only events that describe life-cycle of graphics buffers across
// Android and Chrome. It also includes top level graphics events in Chrome and
// Android. Events in this model have type and timestamp and grouped per each
// view, which is defined by Activity name and Android task id.
// View events are kept separately per individual view and each view may own
// multiple graphics buffers. Following is the structure of events:
// |android_top_level_| - top level rendering events from Android
// |chrome_top_level_| - top level rendering events from Chrome.
// |view_buffers_| - map views to buffer events.
// -- view1
//    -- buffer_1
//       ...
//    -- buffer_n (usually 4 buffers per view)
// -- view2
//       ...
// In normal conditions events are expected to follow the pattern when events
// appear in predefined order. Breaking this sequence usually indicates missing
// frame, junk or another problem with rendering.
class ArcTracingGraphicsModel {
 public:
  enum class BufferEventType {
    kNone,  // 0

    // Surface flinger events.
    kBufferQueueDequeueStart = 100,  // 100
    kBufferQueueDequeueDone,         // 101
    kBufferQueueQueueStart,          // 102
    kBufferQueueQueueDone,           // 103
    kBufferQueueAcquire,             // 104
    kBufferQueueReleased,            // 105
    kBufferFillJank,                 // 106,

    // Wayland exo events
    kExoSurfaceAttach = 200,  // 200
    kExoProduceResource,      // 201
    kExoBound,                // 202
    kExoPendingQuery,         // 203
    kExoReleased,             // 204
    kExoJank,                 // 205
    kExoSurfaceCommit,        // 206

    // Chrome events
    kChromeBarrierOrder = 300,  // 300
    kChromeBarrierFlush,        // 301

    // Android Surface Flinger top level events.
    kSurfaceFlingerVsyncHandler = 400,  // 400
    kSurfaceFlingerInvalidationStart,   // 401
    kSurfaceFlingerInvalidationDone,    // 402
    kSurfaceFlingerCompositionStart,    // 403
    kSurfaceFlingerCompositionDone,     // 404
    kSurfaceFlingerCompositionJank,     // 405,
    kVsyncTimestamp,                    // 406,

    // Chrome OS top level events.
    kChromeOSDraw = 500,        // 500
    kChromeOSSwap,              // 501
    kChromeOSWaitForAck,        // 502
    kChromeOSPresentationDone,  // 503
    kChromeOSSwapDone,          // 504
    kChromeOSJank,              // 505,

    // Custom event.
    kCustomEvent = 600,

    // Input events
    kInputEventCreated = 700,      // 700
    kInputEventWaylandDispatched,  // 701
    kInputEventDeliverStart,       // 702
    kInputEventDeliverEnd,         // 703
  };

  struct BufferEvent {
    BufferEvent(BufferEventType type, int64_t timestamp);
    BufferEvent(BufferEventType type,
                int64_t timestamp,
                const std::string& content);

    bool operator==(const BufferEvent& other) const;

    BufferEventType type;
    uint64_t timestamp;
    std::string content;
  };

  struct ViewId {
    ViewId(int task_id, const std::string& activity);

    bool operator<(const ViewId& other) const;
    bool operator==(const ViewId& other) const;

    int task_id;
    std::string activity;
  };

  using BufferEvents = std::vector<BufferEvent>;

  class EventsContainer {
   public:
    EventsContainer();
    ~EventsContainer();

    void Reset();

    bool operator==(const EventsContainer& other) const;

    std::vector<BufferEvents>& buffer_events() { return buffer_events_; }
    const std::vector<BufferEvents>& buffer_events() const {
      return buffer_events_;
    }
    BufferEvents& global_events() { return global_events_; }
    const BufferEvents& global_events() const { return global_events_; }

   private:
    // Events associated with particular graphics buffer.
    std::vector<BufferEvents> buffer_events_;
    // Global events that do not belong to any graphics buffer.
    BufferEvents global_events_;

    DISALLOW_COPY_AND_ASSIGN(EventsContainer);
  };

  using ViewMap = std::map<ViewId, EventsContainer>;

  ArcTracingGraphicsModel();
  ~ArcTracingGraphicsModel();

  // Trims container events by |trim_timestamp|. All global events are discarded
  // prior to |trim_timestamp|. Buffer events are discarded prior to
  // |trim_timestamp| and on and after until event from |start_types| is
  // detected.
  static void TrimEventsContainer(
      ArcTracingGraphicsModel::EventsContainer* container,
      int64_t trim_timestamp,
      const std::set<ArcTracingGraphicsModel::BufferEventType>& start_types);

  // Builds the model from the common tracing model |common_model|.
  bool Build(const ArcTracingModel& common_model);

  // Serializes the model to |base::DictionaryValue|, this can be passed to
  // javascript for rendering.
  std::unique_ptr<base::DictionaryValue> Serialize() const;
  // Serializes the model to Json string.
  std::string SerializeToJson() const;
  // Loads the model from Json string.
  bool LoadFromJson(const std::string& json_data);
  // Loads the model from |base::DictionaryValue|.
  bool LoadFromValue(const base::DictionaryValue& root);

  uint64_t duration() const { return duration_; }
  base::Time timestamp() const { return timestamp_; }
  const std::string& app_title() const { return app_title_; }
  const std::vector<unsigned char>& app_icon_png() const {
    return app_icon_png_;
  }
  const std::string& platform() const { return platform_; }

  const ViewMap& view_buffers() const { return view_buffers_; }

  const EventsContainer& android_top_level() const {
    return android_top_level_;
  }

  const EventsContainer& chrome_top_level() const { return chrome_top_level_; }

  const EventsContainer& input() const { return input_; }

  ArcSystemModel& system_model() { return system_model_; }
  const ArcSystemModel& system_model() const { return system_model_; }

  void set_timestamp(base::Time timestamp) { timestamp_ = timestamp; }
  void set_app_title(const std::string& app_title) { app_title_ = app_title; }
  void set_app_icon_png(const std::vector<unsigned char>& app_icon_png) {
    app_icon_png_ = app_icon_png;
  }
  void set_platform(const std::string& platform) { platform_ = platform; }

  void set_skip_structure_validation() { skip_structure_validation_ = true; }

 private:
  // Normalizes timestamp for all events by subtracting the timestamp of the
  // earliest event.
  void NormalizeTimestamps();

  // Resets whole model.
  void Reset();

  // Trims events before first VSYNC event. ARC tracing starts delayed in
  // comparison with Chrome, memory and CPU events. That makes empty area for
  // graphics buffer confusing.
  void VsyncTrim();

  // Extracts task id from the Chrome buffer name. Returns -1 if task id cannot
  // be extracted.
  int GetTaskIdFromBufferName(const std::string& chrome_buffer_name) const;

  ViewMap view_buffers_;
  // To avoid overlapping events are stored interlaced.
  EventsContainer chrome_top_level_;
  EventsContainer android_top_level_;
  EventsContainer input_;
  // Total duration of this model.
  uint32_t duration_ = 0;
  // Title of the traced app.
  std::string app_title_;
  // PNG content of traced app.
  std::vector<unsigned char> app_icon_png_;
  // Tested platform, it includes board, and versions.
  std::string platform_;
  // Timestamp of tracing.
  base::Time timestamp_;

  // Map Chrome buffer id to task id.
  std::map<std::string, int> chrome_buffer_id_to_task_id_;
  // CPU event model.
  ArcSystemModel system_model_;
  // Allows to have model incomplete, used in overview and in tests.
  bool skip_structure_validation_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingGraphicsModel);
};

std::ostream& operator<<(std::ostream& os,
                         ArcTracingGraphicsModel::BufferEventType);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_GRAPHICS_MODEL_H_
