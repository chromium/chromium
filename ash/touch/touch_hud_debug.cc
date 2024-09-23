// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/touch/touch_hud_debug.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

const int kPointRadius = 20;
const SkColor kColors[] = {
    SK_ColorYELLOW,
    SK_ColorGREEN,
    SK_ColorRED,
    SK_ColorBLUE,
    SK_ColorGRAY,
    SK_ColorMAGENTA,
    SK_ColorCYAN,
    SK_ColorWHITE,
    SK_ColorBLACK,
    SkColorSetRGB(0xFF, 0x8C, 0x00),
    SkColorSetRGB(0x8B, 0x45, 0x13),
    SkColorSetRGB(0xFF, 0xDE, 0xAD),
};
const int kAlpha = 0x60;
const int kMaxPaths = std::size(kColors);
const int kReducedScale = 10;

const char* GetTouchEventLabel(ui::EventType type) {
  switch (type) {
    case ui::EventType::kUnknown:
      return " ";
    case ui::EventType::kTouchPressed:
      return "P";
    case ui::EventType::kTouchMoved:
      return "M";
    case ui::EventType::kTouchReleased:
      return "R";
    case ui::EventType::kTouchCancelled:
      return "C";
    default:
      break;
  }
  return "?";
}

// A TouchPointLog represents a single touch-event of a touch point.
struct TouchPointLog {
 public:
  explicit TouchPointLog(const ui::TouchEvent& touch)
      : type(touch.type()),
        location(touch.root_location()),
        radius_x(touch.pointer_details().radius_x),
        radius_y(touch.pointer_details().radius_y) {}

  ui::EventType type;
  gfx::Point location;
  float radius_x;
  float radius_y;
};

// A TouchTrace keeps track of all the touch events of a single touch point
// (starting from a touch-press and ending at a touch-release or touch-cancel).
class TouchTrace {
 public:
  typedef std::vector<TouchPointLog>::iterator iterator;
  typedef std::vector<TouchPointLog>::const_iterator const_iterator;
  typedef std::vector<TouchPointLog>::reverse_iterator reverse_iterator;
  typedef std::vector<TouchPointLog>::const_reverse_iterator
      const_reverse_iterator;

  TouchTrace() = default;

  TouchTrace(const TouchTrace&) = delete;
  TouchTrace& operator=(const TouchTrace&) = delete;

  void AddTouchPoint(const ui::TouchEvent& touch) {
    log_.push_back(TouchPointLog(touch));
  }

  const std::vector<TouchPointLog>& log() const { return log_; }

  bool active() const {
    return !log_.empty() && log_.back().type != ui::EventType::kTouchReleased &&
           log_.back().type != ui::EventType::kTouchCancelled;
  }

  void Reset() { log_.clear(); }

 private:
  std::vector<TouchPointLog> log_;
};

// A TouchLog keeps track of all touch events of all touch points.
class TouchLog {
 public:
  TouchLog() : next_trace_index_(0) {}

  TouchLog(const TouchLog&) = delete;
  TouchLog& operator=(const TouchLog&) = delete;

  void AddTouchPoint(const ui::TouchEvent& touch) {
    if (touch.type() == ui::EventType::kTouchPressed) {
      StartTrace(touch);
    }
    AddToTrace(touch);
  }

  void Reset() {
    next_trace_index_ = 0;
    for (int i = 0; i < kMaxPaths; ++i)
      traces_[i].Reset();
  }

  int GetTraceIndex(int touch_id) const {
    return touch_id_to_trace_index_.at(touch_id);
  }

  const TouchTrace* traces() const { return traces_; }

 private:
  void StartTrace(const ui::TouchEvent& touch) {
    // Find the first inactive spot; otherwise, overwrite the one
    // |next_trace_index_| is pointing to.
    int old_trace_index = next_trace_index_;
    do {
      if (!traces_[next_trace_index_].active())
        break;
      next_trace_index_ = (next_trace_index_ + 1) % kMaxPaths;
    } while (next_trace_index_ != old_trace_index);
    int touch_id = touch.pointer_details().id;
    traces_[next_trace_index_].Reset();
    touch_id_to_trace_index_[touch_id] = next_trace_index_;
    next_trace_index_ = (next_trace_index_ + 1) % kMaxPaths;
  }

  void AddToTrace(const ui::TouchEvent& touch) {
    int touch_id = touch.pointer_details().id;
    int trace_index = touch_id_to_trace_index_[touch_id];
    traces_[trace_index].AddTouchPoint(touch);
  }

  TouchTrace traces_[kMaxPaths];
  int next_trace_index_;

  std::map<int, int> touch_id_to_trace_index_;
};

// TouchHudCanvas draws touch traces in |FULLSCREEN| and |REDUCED_SCALE| modes.
class TouchHudCanvas : public views::View {
  METADATA_HEADER(TouchHudCanvas, views::View)

 public:
  explicit TouchHudCanvas(const TouchLog& touch_log)
      : touch_log_(touch_log), scale_(1) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    flags_.setStyle(cc::PaintFlags::kFill_Style);
  }

  TouchHudCanvas(const TouchHudCanvas&) = delete;
  TouchHudCanvas& operator=(const TouchHudCanvas&) = delete;

  ~TouchHudCanvas() override = default;

  void SetScale(int scale) {
    if (scale_ == scale)
      return;
    scale_ = scale;
    gfx::Transform transform;
    transform.Scale(1. / scale_, 1. / scale_);
    layer()->SetTransform(transform);
  }

  int scale() const { return scale_; }

  void TouchPointAdded(int touch_id) {
    int trace_index = touch_log_->GetTraceIndex(touch_id);
    const TouchTrace& trace = touch_log_->traces()[trace_index];
    const TouchPointLog& point = trace.log().back();
    if (point.type == ui::EventType::kTouchPressed) {
      StartedTrace(trace_index);
    }
    if (point.type != ui::EventType::kTouchCancelled) {
      AddedPointToTrace(trace_index);
    }
  }

  void Clear() {
    for (int i = 0; i < kMaxPaths; ++i)
      paths_[i].reset();

    SchedulePaint();
  }

 private:
  void StartedTrace(int trace_index) {
    paths_[trace_index].reset();
    colors_[trace_index] = SkColorSetA(kColors[trace_index], kAlpha);
  }

  void AddedPointToTrace(int trace_index) {
    const TouchTrace& trace = touch_log_->traces()[trace_index];
    const TouchPointLog& point = trace.log().back();
    const gfx::Point& location = point.location;
    SkScalar x = SkIntToScalar(location.x());
    SkScalar y = SkIntToScalar(location.y());
    SkPoint last;
    if (!paths_[trace_index].getLastPt(&last) || x != last.x() ||
        y != last.y()) {
      paths_[trace_index].addCircle(x, y, SkIntToScalar(kPointRadius));
      SchedulePaint();
    }
  }

  // Overridden from views::View.
  void OnPaint(gfx::Canvas* canvas) override {
    for (int i = 0; i < kMaxPaths; ++i) {
      if (paths_[i].countPoints() == 0)
        continue;
      flags_.setColor(colors_[i]);
      canvas->DrawPath(paths_[i], flags_);
    }
  }

  cc::PaintFlags flags_;

  const raw_ref<const TouchLog, DanglingUntriaged> touch_log_;
  SkPath paths_[kMaxPaths];
  SkColor colors_[kMaxPaths];

  int scale_;
};

BEGIN_METADATA(TouchHudCanvas)
END_METADATA

TouchHudDebug::TouchHudDebug(aura::Window* initial_root)
    : TouchObserverHud(initial_root, "TouchHudDebug"),
      mode_(FULLSCREEN),
      touch_log_(new TouchLog()),
      canvas_(new TouchHudCanvas(*touch_log_)),
      label_container_(new views::View()) {
  const display::Display& display =
      Shell::Get()->display_manager()->GetDisplayForId(display_id());

  views::View* content = widget()->GetContentsView();

  content->AddChildView(canvas_.get());

  const gfx::Size& display_size = display.size();
  canvas_->SetSize(display_size);

  label_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  constexpr SkColor kShadowColor = SK_ColorWHITE;
  const SkColor label_color =
      color_utils::GetColorWithMaxContrast(kShadowColor);
  for (int i = 0; i < kMaxTouchPoints; ++i) {
    touch_labels_[i] = new views::Label;
    touch_labels_[i]->SetEnabledColor(label_color);
    touch_labels_[i]->SetBackgroundColor(SK_ColorTRANSPARENT);
    touch_labels_[i]->SetShadows(gfx::ShadowValues(
        1, gfx::ShadowValue(gfx::Vector2d(1, 1), 0, kShadowColor)));
    label_container_->AddChildView(touch_labels_[i]);
  }
  label_container_->SetX(0);
  label_container_->SetY(display_size.height() / kReducedScale);
  label_container_->SetSize(label_container_->GetPreferredSize());
  label_container_->SetVisible(false);
  content->AddChildView(label_container_.get());
}

TouchHudDebug::~TouchHudDebug() = default;

void TouchHudDebug::ChangeToNextMode() {
  switch (mode_) {
    case FULLSCREEN:
      SetMode(REDUCED_SCALE);
      break;
    case REDUCED_SCALE:
      SetMode(INVISIBLE);
      break;
    case INVISIBLE:
      SetMode(FULLSCREEN);
      break;
  }
}

void TouchHudDebug::Clear() {
  if (widget()->IsVisible()) {
    canvas_->Clear();
    for (int i = 0; i < kMaxTouchPoints; ++i)
      touch_labels_[i]->SetText(std::u16string());
    label_container_->SetSize(label_container_->GetPreferredSize());
  }
}

void TouchHudDebug::SetMode(Mode mode) {
  if (mode_ == mode)
    return;
  mode_ = mode;
  switch (mode) {
    case FULLSCREEN:
      label_container_->SetVisible(false);
      canvas_->SetVisible(true);
      canvas_->SetScale(1);
      canvas_->SchedulePaint();
      widget()->Show();
      break;
    case REDUCED_SCALE:
      label_container_->SetVisible(true);
      canvas_->SetVisible(true);
      canvas_->SetScale(kReducedScale);
      canvas_->SchedulePaint();
      widget()->Show();
      break;
    case INVISIBLE:
      widget()->Hide();
      break;
  }
}

void TouchHudDebug::UpdateTouchPointLabel(int index) {
  int trace_index = touch_log_->GetTraceIndex(index);
  const TouchTrace& trace = touch_log_->traces()[trace_index];
  TouchTrace::const_reverse_iterator point = trace.log().rbegin();
  ui::EventType touch_status = point->type;
  float touch_radius = std::max(point->radius_x, point->radius_y);
  while (point != trace.log().rend() &&
         point->type == ui::EventType::kTouchCancelled) {
    point++;
  }
  DCHECK(point != trace.log().rend());
  gfx::Point touch_position = point->location;

  std::string string = base::StringPrintf(
      "%2d: %s %s (%.4f)", index, GetTouchEventLabel(touch_status),
      touch_position.ToString().c_str(), touch_radius);
  touch_labels_[index]->SetText(base::UTF8ToUTF16(string));
}

void TouchHudDebug::OnTouchEvent(ui::TouchEvent* event) {
  if (event->pointer_details().id >= kMaxTouchPoints)
    return;

  touch_log_->AddTouchPoint(*event);
  canvas_->TouchPointAdded(event->pointer_details().id);
  UpdateTouchPointLabel(event->pointer_details().id);
  label_container_->SetSize(label_container_->GetPreferredSize());
}

void TouchHudDebug::OnDisplayMetricsChanged(const display::Display& display,
                                            uint32_t metrics) {
  TouchObserverHud::OnDisplayMetricsChanged(display, metrics);

  if (display.id() != display_id() || !(metrics & DISPLAY_METRIC_BOUNDS))
    return;
  const gfx::Size& size = display.size();
  canvas_->SetSize(size);
  label_container_->SetY(size.height() / kReducedScale);
}

void TouchHudDebug::SetHudForRootWindowController(
    RootWindowController* controller) {
  controller->set_touch_hud_debug(this);
}

void TouchHudDebug::UnsetHudForRootWindowController(
    RootWindowController* controller) {
  controller->set_touch_hud_debug(NULL);
}

}  // namespace ash
