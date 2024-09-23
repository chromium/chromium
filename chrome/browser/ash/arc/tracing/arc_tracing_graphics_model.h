// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_GRAPHICS_MODEL_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_GRAPHICS_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/tracing/arc_system_model.h"

namespace arc {

//
// Keys for reading the model JSON.
//

// Key for summary data in <top>.information.
inline constexpr char kKeyInformation[] = "information";

// Keys in information JSON.
inline constexpr char kKeyDuration[] = "duration";
inline constexpr char kKeyPerceivedFps[] = "perceived_fps";

class ArcTracingModel;
class PresentFramesTracer;

// Graphic buffers events model. It is build from the generic |ArcTracingModel|
// and contains only events that describe life-cycle of graphics buffers across
// Android and Chrome. It also includes top level graphics events in Chrome and
// Android. Events in this model have type and timestamp and grouped per each
// view, which is defined by Activity name and Android task id.
// View events are kept separately per individual view and each view may own
// multiple graphics buffers. Following is the structure of events:
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
  // 'Obsolete' indicates a constant is only here to document legacy traces,
  // especially those in test data. Such items are not included in new traces.
  // When adding or editing lines, prefer an actual "= ###" rather than rely
  // on implicit incrementing, and do not change constant values once added.
  // clang-format off
  enum class EventType {
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
    kExoSurfaceAttach     = 200,  // Obsolete
    kExoProduceResource   = 201,  // Obsolete
    kExoBound             = 202,  // Obsolete
    kExoPendingQuery      = 203,  // Obsolete
    kExoReleased          = 204,  // Obsolete
    kExoJank              = 205,
    kExoSurfaceCommit     = 206,
    kExoSurfaceCommitJank = 207,
    kExoLastEvent         = kExoSurfaceCommitJank,

    // Chrome events
    kChromeBarrierOrder = 300,  // Obsolete
    kChromeBarrierFlush = 301,  // Obsolete

    // Android Surface Flinger top level events.
    kSurfaceFlingerVsyncHandler      = 400,  // Obsolete
    kSurfaceFlingerInvalidationStart = 401,
    kSurfaceFlingerInvalidationDone  = 402,
    kSurfaceFlingerCompositionStart  = 403,
    kSurfaceFlingerCompositionDone   = 404,
    kSurfaceFlingerCompositionJank   = 405,  // Obsolete
    kVsyncTimestamp                  = 406,  // Obsolete

    // Chrome OS top level events.
    kChromeOSDraw             = 500,  // Obsolete
    kChromeOSSwap             = 501,  // Obsolete
    kChromeOSWaitForAck       = 502,  // Obsolete
    kChromeOSPresentationDone = 503,
    kChromeOSSwapDone         = 504,
    kChromeOSJank             = 505,  // Obsolete
    kChromeOSPerceivedJank    = 506,
    kChromeOSSwapJank         = 507,
    kChromeOSLastEvent        = kChromeOSSwapJank,

    // Custom event.
    kCustomEvent = 600,  // Obsolete

    // Input events
    kInputEventCreated           = 700,  // Obsolete
    kInputEventWaylandDispatched = 701,  // Obsolete
    kInputEventDeliverStart      = 702,  // Obsolete
    kInputEventDeliverEnd        = 703,  // Obsolete
  };
  // clang-format on

  struct BufferEvent {
    BufferEvent(EventType type, int64_t timestamp);
    BufferEvent(EventType type, int64_t timestamp, const std::string& content);

    bool operator==(const BufferEvent& other) const;

    EventType type;
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

    EventsContainer(const EventsContainer&) = delete;
    EventsContainer& operator=(const EventsContainer&) = delete;

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
  };

  using ViewMap = std::map<ViewId, EventsContainer>;

  ArcTracingGraphicsModel();

  ArcTracingGraphicsModel(const ArcTracingGraphicsModel&) = delete;
  ArcTracingGraphicsModel& operator=(const ArcTracingGraphicsModel&) = delete;

  ~ArcTracingGraphicsModel();

  // Builds the model from the common tracing model |common_model|.
  bool Build(const ArcTracingModel& common_model,
             const PresentFramesTracer& present_frames);

  // Serializes the model to |base::Value::Dict|, this can be passed to
  // javascript for rendering.
  base::Value::Dict Serialize() const;
  // Serializes the model to Json string.
  std::string SerializeToJson() const;
  // Loads the model from Json string.
  bool LoadFromJson(const std::string& json_data);
  // Loads the model from |base::Value::Dict|.
  bool LoadFromValue(const base::Value::Dict& root);

  uint64_t duration() const { return duration_; }
  base::Time timestamp() const { return timestamp_; }
  const std::string& app_title() const { return app_title_; }
  const std::vector<unsigned char>& app_icon_png() const {
    return app_icon_png_;
  }
  const std::string& platform() const { return platform_; }

  const ViewMap& view_buffers() const { return view_buffers_; }

  const EventsContainer& chrome_top_level() const { return chrome_top_level_; }

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

  ViewMap view_buffers_;
  // To avoid overlapping events are stored interlaced.
  EventsContainer chrome_top_level_;
  // Total duration of this model in microseconds.
  uint32_t duration_ = 0;
  // Effective FPS - counts only swapped frames with new app commits.
  double perceived_fps_ = 0;
  // App FPS, which is the number of commits from ARC Wayland client per second.
  double app_fps_ = 0;
  // Title of the traced app.
  std::string app_title_;
  // PNG content of traced app.
  std::vector<unsigned char> app_icon_png_;
  // Tested platform, it includes board, and versions.
  std::string platform_;
  // Timestamp of tracing.
  base::Time timestamp_;

  // CPU event model.
  ArcSystemModel system_model_;
  // Allows to have model incomplete, used in overview and in tests.
  bool skip_structure_validation_ = false;
};

std::ostream& operator<<(std::ostream& os, ArcTracingGraphicsModel::EventType);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_TRACING_GRAPHICS_MODEL_H_
