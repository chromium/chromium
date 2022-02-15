// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_move_mouse.h"

#include "ash/shell.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace arc {
namespace input_overlay {
namespace {
// About Json strings.
constexpr char kMouseAction[] = "mouse_action";
constexpr char kHoverMove[] = "hover_move";
constexpr char kPrimaryDragMove[] = "primary_drag_move";
constexpr char kSecondaryDragMove[] = "secondary_drag_move";
constexpr char kTargetArea[] = "target_area";
constexpr char kTopLeft[] = "top_left";
constexpr char kBottomRight[] = "bottom_right";

std::unique_ptr<Position> ParseApplyAreaPosition(const base::Value& value,
                                                 base::StringPiece key) {
  auto* point = value.FindDictKey(key);
  if (!point) {
    LOG(ERROR) << "Apply area in mouse move action requires: " << key;
    return nullptr;
  }
  auto pos = ParsePosition(*point);
  if (!pos) {
    LOG(ERROR) << "Not valid position for: " << key;
    return nullptr;
  }
  return pos;
}

}  // namespace

class ActionMoveMouse::ActionMoveMouseView : public ActionView {
 public:
  ActionMoveMouseView(Action* action, const gfx::RectF& content_bounds)
      : ActionView(action) {
    auto label = std::make_unique<ActionLabel>(u"mouse cursor lock (esc)");
    label->set_editable(false);
    auto label_size = label->GetPreferredSize();
    label->SetSize(label_size);
    SetSize(label_size);
    center_.set_x(label_size.width() / 2);
    center_.set_y(label_size.height() / 2);
    labels_.emplace_back(AddChildView(std::move(label)));
  }

  ActionMoveMouseView(const ActionMoveMouseView&) = delete;
  ActionMoveMouseView& operator=(const ActionMoveMouseView&) = delete;
  ~ActionMoveMouseView() override = default;
};

ActionMoveMouse::ActionMoveMouse(aura::Window* window) : Action(window) {}

ActionMoveMouse::~ActionMoveMouse() = default;

bool ActionMoveMouse::ParseFromJson(const base::Value& value) {
  Action::ParseFromJson(value);

  const auto* mouse_action = value.FindStringKey(kMouseAction);
  if (!mouse_action) {
    LOG(ERROR) << "Must include mouse action for mouse move action.";
    return false;
  }
  require_mouse_locked_ = true;
  target_mouse_action_ = *mouse_action;
  if (target_mouse_action_ == kHoverMove) {
    target_types_.emplace(ui::ET_MOUSE_ENTERED);
    target_types_.emplace(ui::ET_MOUSE_MOVED);
    target_types_.emplace(ui::ET_MOUSE_EXITED);
  } else if (target_mouse_action_ == kPrimaryDragMove ||
             target_mouse_action_ == kSecondaryDragMove) {
    target_types_.emplace(ui::ET_MOUSE_PRESSED);
    target_types_.emplace(ui::ET_MOUSE_DRAGGED);
    target_types_.emplace(ui::ET_MOUSE_RELEASED);
    if (target_mouse_action_ == kPrimaryDragMove)
      target_flags_ = ui::EF_LEFT_MOUSE_BUTTON;
    else
      target_flags_ = ui::EF_RIGHT_MOUSE_BUTTON;
  } else {
    LOG(ERROR) << "Not supported mouse action: " << target_mouse_action_;
    return false;
  }

  auto* target_area = value.FindDictKey(kTargetArea);
  if (target_area) {
    auto top_left = ParseApplyAreaPosition(*target_area, kTopLeft);
    if (!top_left)
      return false;
    auto bottom_right = ParseApplyAreaPosition(*target_area, kBottomRight);
    if (!bottom_right)
      return false;

    // Verify |top_left| is located on the top-left of the |bottom_right|. Use a
    // random positive window content bounds to test it.
    auto temp_rect = gfx::RectF(10, 10, 100, 100);
    auto top_left_point = top_left->CalculatePosition(temp_rect);
    auto bottom_right_point = bottom_right->CalculatePosition(temp_rect);
    if (top_left_point.x() > bottom_right_point.x() - 1 ||
        top_left_point.y() > bottom_right_point.y() - 1) {
      LOG(ERROR) << "Apply area in mouse move action is not verified. For "
                    "window content bounds "
                 << temp_rect.ToString() << ", get top-left position "
                 << top_left_point.ToString() << " and bottom-right position "
                 << bottom_right_point.ToString()
                 << ". Top-left position should be on the top-left of the "
                    "bottom-right position.";
      return false;
    }

    target_area_.emplace_back(std::move(top_left));
    target_area_.emplace_back(std::move(bottom_right));
  }

  return true;
}

bool ActionMoveMouse::RewriteEvent(const ui::Event& origin,
                                   const gfx::RectF& content_bounds,
                                   const bool is_mouse_locked,
                                   std::list<ui::TouchEvent>& touch_events,
                                   bool& keep_original_event) {
  if (!origin.IsMouseEvent() || !is_mouse_locked)
    return false;

  LogEvent(origin);
  auto* mouse_event = origin.AsMouseEvent();
  auto rewritten = RewriteMouseEvent(mouse_event, content_bounds,
                                     is_mouse_locked, touch_events);
  LogTouchEvents(touch_events);
  return rewritten;
}

gfx::PointF ActionMoveMouse::GetUICenterPosition(
    const gfx::RectF& content_bounds) {
  if (locations().empty())
    return gfx::PointF(content_bounds.width() / 2, content_bounds.height() / 2);
  // Sometimes, the touch down position binds to a UI location. The action view
  // shows on the UI location. If it's null, the touch down position is
  // anywhere. The action view shows in the center of the window.
  return locations().front().get()->CalculatePosition(content_bounds);
}

std::unique_ptr<ActionView> ActionMoveMouse::CreateView(
    const gfx::RectF& content_bounds) {
  auto view = std::make_unique<ActionMoveMouseView>(this, content_bounds);
  view->set_editable(false);
  auto center_pos = GetUICenterPosition(content_bounds);
  view->SetPositionFromCenterPosition(center_pos);
  return view;
}

bool ActionMoveMouse::RewriteMouseEvent(
    const ui::MouseEvent* mouse_event,
    const gfx::RectF& content_bounds,
    const bool is_mouse_locked,
    std::list<ui::TouchEvent>& rewritten_events) {
  DCHECK(mouse_event);

  auto type = mouse_event->type();
  if (!target_types_.contains(type) || target_flags_ != mouse_event->flags())
    return false;

  auto mouse_location = gfx::Point(mouse_event->root_location());
  target_window_->GetHost()->ConvertPixelsToDIP(&mouse_location);
  auto mouse_location_f = gfx::PointF(mouse_location);
  // Discard mouse events outside of the app content bounds if the mouse is
  // locked.
  if (!content_bounds.Contains(mouse_location_f))
    return true;

  last_touch_root_location_ =
      TransformLocationInPixels(content_bounds, mouse_location_f);

  if (type == ui::ET_MOUSE_ENTERED || type == ui::ET_MOUSE_PRESSED)
    DCHECK(!touch_id_);
  // Mouse might be unlocked before ui::ET_MOUSE_EXITED, so no need to check
  // ui::ET_MOUSE_EXITED.
  if (type == ui::ET_MOUSE_RELEASED)
    DCHECK(touch_id_);
  if (!touch_id_) {
    touch_id_ = TouchIdManager::GetInstance()->ObtainTouchID();
    auto touch_down_pos = CalculateTouchPosition(content_bounds);
    if (touch_down_pos)
      last_touch_root_location_ = *touch_down_pos;
    rewritten_events.emplace_back(
        ui::EventType::ET_TOUCH_PRESSED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_));
  } else if (type == ui::ET_MOUSE_EXITED || type == ui::ET_MOUSE_RELEASED) {
    rewritten_events.emplace_back(
        ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_));
    OnTouchReleased();
  } else {
    DCHECK(type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED);
    rewritten_events.emplace_back(
        ui::EventType::ET_TOUCH_MOVED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_));
  }
  ui::Event::DispatcherApi(&(rewritten_events.back()))
      .set_target(target_window_);
  return true;
}

absl::optional<gfx::RectF> ActionMoveMouse::CalculateApplyArea(
    const gfx::RectF& content_bounds) {
  if (target_area_.size() != 2)
    return absl::nullopt;

  auto top_left = target_area_[0]->CalculatePosition(content_bounds);
  auto bottom_right = target_area_[1]->CalculatePosition(content_bounds);
  return absl::make_optional<gfx::RectF>(
      top_left.x() + content_bounds.x(), top_left.y() + content_bounds.y(),
      bottom_right.x() - top_left.x(), bottom_right.y() - top_left.y());
}

gfx::PointF ActionMoveMouse::TransformLocationInPixels(
    const gfx::RectF& content_bounds,
    const gfx::PointF& root_location) {
  auto target_area = CalculateApplyArea(content_bounds);
  auto new_pos = gfx::PointF();
  if (target_area) {
    auto orig_point = root_location - content_bounds.origin();
    float ratio = orig_point.x() / content_bounds.width();
    float x = ratio * target_area->width() + target_area->x();
    ratio = orig_point.y() / content_bounds.height();
    float y = ratio * target_area->height() + target_area->y();
    new_pos.SetPoint(x, y);
    DCHECK(target_area->Contains(new_pos));
  } else {
    new_pos.SetPoint(root_location.x(), root_location.y());
  }

  float scale = target_window_->GetHost()->device_scale_factor();
  new_pos.Scale(scale);
  return new_pos;
}

}  // namespace input_overlay
}  // namespace arc
