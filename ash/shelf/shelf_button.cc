// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/ash_constants.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"

namespace {

constexpr int kStatusIndicatorAttentionThrobDurationMS = 800;
constexpr int kStatusIndicatorMaxAnimationSeconds = 10;
constexpr int kStatusIndicatorRadiusDip = 2;
constexpr int kStatusIndicatorMaxSize = 10;
constexpr int kStatusIndicatorActiveSize = 8;
constexpr int kStatusIndicatorRunningSize = 4;
constexpr int kStatusIndicatorThickness = 2;
constexpr int kNotificationIndicatorRadiusDip = 7;
constexpr SkColor kIndicatorColor = SK_ColorWHITE;

// Slightly different colors and alpha in the new UI.
constexpr SkColor kIndicatorColorActive = kIndicatorColor;
constexpr SkColor kIndicatorColorRunning = SkColorSetA(SK_ColorWHITE, 0x7F);

// Shelf item ripple size.
constexpr int kInkDropLargeSize = 60;

// The time threshold before an item can be dragged.
constexpr int kDragTimeThresholdMs = 300;

// The time threshold before the ink drop should activate on a long press.
constexpr int kInkDropRippleActivationTimeMs = 650;

// The drag and drop app icon should get scaled by this factor.
constexpr float kAppIconScale = 1.2f;

// The drag and drop app icon scaling up or down animation transition duration.
constexpr int kDragDropAppIconScaleTransitionMs = 200;

// Simple AnimationDelegate that owns a single ThrobAnimation instance to
// keep all Draw Attention animations in sync.
class ShelfButtonAnimation : public gfx::AnimationDelegate {
 public:
  class Observer {
   public:
    virtual void AnimationProgressed() = 0;

   protected:
    virtual ~Observer() = default;
  };

  static ShelfButtonAnimation* GetInstance() {
    static ShelfButtonAnimation* s_instance = new ShelfButtonAnimation();
    return s_instance;
  }

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
    if (!observers_.might_have_observers())
      animation_.Stop();
  }

  bool HasObserver(Observer* observer) const {
    return observers_.HasObserver(observer);
  }

  SkAlpha GetAlpha() {
    return GetThrobAnimation().CurrentValueBetween(SK_AlphaTRANSPARENT,
                                                   SK_AlphaOPAQUE);
  }

  double GetAnimation() { return GetThrobAnimation().GetCurrentValue(); }

 private:
  ShelfButtonAnimation() : animation_(this) {
    animation_.SetThrobDuration(kStatusIndicatorAttentionThrobDurationMS);
    animation_.SetTweenType(gfx::Tween::SMOOTH_IN_OUT);
  }

  ~ShelfButtonAnimation() override = default;

  gfx::ThrobAnimation& GetThrobAnimation() {
    if (!animation_.is_animating()) {
      animation_.Reset();
      animation_.StartThrobbing(-1 /*throb indefinitely*/);
    }
    return animation_;
  }

  // gfx::AnimationDelegate
  void AnimationProgressed(const gfx::Animation* animation) override {
    if (animation != &animation_)
      return;
    if (!animation_.is_animating())
      return;
    for (auto& observer : observers_)
      observer.AnimationProgressed();
  }

  gfx::ThrobAnimation animation_;
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(ShelfButtonAnimation);
};

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ShelfButton::AppNotificationIndicatorView

// The indicator which is activated when the app corresponding with this
// ShelfButton recieves a notification.
class ShelfButton::AppNotificationIndicatorView : public views::View {
 public:
  explicit AppNotificationIndicatorView(SkColor indicator_color)
      : indicator_color_(indicator_color) {}

  ~AppNotificationIndicatorView() override {}

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped(canvas);

    canvas->SaveLayerAlpha(SK_AlphaOPAQUE);

    DCHECK_EQ(width(), height());
    DCHECK_EQ(kNotificationIndicatorRadiusDip, width() / 2);
    const float dsf = canvas->UndoDeviceScaleFactor();
    const int kStrokeWidthPx = 1;
    gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
    center.Scale(dsf);

    // Fill the center.
    cc::PaintFlags flags;
    flags.setColor(indicator_color_);
    flags.setAntiAlias(true);
    canvas->DrawCircle(
        center, dsf * kNotificationIndicatorRadiusDip - kStrokeWidthPx, flags);

    // Stroke the border.
    flags.setColor(SkColorSetA(SK_ColorBLACK, 0x4D));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    canvas->DrawCircle(
        center, dsf * kNotificationIndicatorRadiusDip - kStrokeWidthPx / 2.0f,
        flags);
  }

 private:
  const SkColor indicator_color_;

  DISALLOW_COPY_AND_ASSIGN(AppNotificationIndicatorView);
};

////////////////////////////////////////////////////////////////////////////////
// ShelfButton::AppStatusIndicatorView

class ShelfButton::AppStatusIndicatorView
    : public views::View,
      public ShelfButtonAnimation::Observer {
 public:
  AppStatusIndicatorView() : show_attention_(false), active_(false) {
    // Make sure the events reach the parent view for handling.
    set_can_process_events_within_subtree(false);
  }

  ~AppStatusIndicatorView() override {
    ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped(canvas);
    if (show_attention_) {
      const SkAlpha alpha =
          ShelfButtonAnimation::GetInstance()->HasObserver(this)
              ? ShelfButtonAnimation::GetInstance()->GetAlpha()
              : SK_AlphaOPAQUE;
      canvas->SaveLayerAlpha(alpha);
    }

    const float dsf = canvas->UndoDeviceScaleFactor();
    gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
    cc::PaintFlags flags;
    // Active and running indicators look a little different in the new UI.
    flags.setColor(active_ ? kIndicatorColorActive : kIndicatorColorRunning);
    flags.setAntiAlias(true);
    flags.setStrokeCap(cc::PaintFlags::Cap::kRound_Cap);
    flags.setStrokeJoin(cc::PaintFlags::Join::kRound_Join);
    flags.setStrokeWidth(kStatusIndicatorThickness);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    float stroke_length =
        active_ ? kStatusIndicatorActiveSize : kStatusIndicatorRunningSize;
    gfx::PointF start;
    gfx::PointF end;
    if (horizontal_shelf_) {
      start = gfx::PointF(center.x() - stroke_length / 2, center.y());
      end = start;
      end.Offset(stroke_length, 0);
    } else {
      start = gfx::PointF(center.x(), center.y() - stroke_length / 2);
      end = start;
      end.Offset(0, stroke_length);
    }
    gfx::Path path;
    path.moveTo(start.x() * dsf, start.y() * dsf);
    path.lineTo(end.x() * dsf, end.y() * dsf);
    canvas->DrawPath(path, flags);
  }

  // ShelfButtonAnimation::Observer
  void AnimationProgressed() override {
    UpdateAnimating();
    SchedulePaint();
  }

  void ShowAttention(bool show) {
    if (show_attention_ == show)
      return;

    show_attention_ = show;
    if (show_attention_) {
      animation_end_time_ =
          base::TimeTicks::Now() +
          base::TimeDelta::FromSeconds(kStatusIndicatorMaxAnimationSeconds);
      ShelfButtonAnimation::GetInstance()->AddObserver(this);
    } else {
      ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
    }
  }

  void ShowActiveStatus(bool active) {
    if (active_ == active)
      return;
    active_ = active;
    SchedulePaint();
  }

  void SetHorizontalShelf(bool horizontal_shelf) {
    if (horizontal_shelf_ == horizontal_shelf)
      return;
    horizontal_shelf_ = horizontal_shelf;
    SchedulePaint();
  }

 private:
  void UpdateAnimating() {
    if (base::TimeTicks::Now() > animation_end_time_)
      ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  bool show_attention_ = false;
  bool active_ = false;
  bool horizontal_shelf_ = true;
  base::TimeTicks animation_end_time_;  // For attention throbbing underline.

  DISALLOW_COPY_AND_ASSIGN(AppStatusIndicatorView);
};

////////////////////////////////////////////////////////////////////////////////
// ShelfButton

// static
const char ShelfButton::kViewClassName[] = "ash/ShelfButton";

ShelfButton::ShelfButton(InkDropButtonListener* listener, ShelfView* shelf_view)
    : Button(nullptr),
      listener_(listener),
      shelf_view_(shelf_view),
      icon_view_(new views::ImageView()),
      indicator_(new AppStatusIndicatorView()),
      notification_indicator_(nullptr),
      state_(STATE_NORMAL),
      destroyed_flag_(nullptr),
      is_notification_indicator_enabled_(
          features::IsNotificationIndicatorEnabled()) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
  set_hide_ink_drop_when_showing_context_menu(false);
  const gfx::ShadowValue kShadows[] = {
      gfx::ShadowValue(gfx::Vector2d(0, 2), 0, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 3), 1, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 0), 1, SkColorSetARGB(0x54, 0, 0, 0)),
  };
  icon_shadows_.assign(kShadows, kShadows + arraysize(kShadows));

  // TODO: refactor the layers so each button doesn't require 3.
  // |icon_view_| needs its own layer so it can be scaled up independently of
  // the ink drop ripple.
  icon_view_->SetPaintToLayer();
  icon_view_->layer()->SetFillsBoundsOpaquely(false);
  icon_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  icon_view_->SetVerticalAlignment(views::ImageView::LEADING);
  // Do not make this interactive, so that events are sent to ShelfView.
  icon_view_->set_can_process_events_within_subtree(false);

  AddChildView(indicator_);
  AddChildView(icon_view_);
  if (is_notification_indicator_enabled_) {
    notification_indicator_ = new AppNotificationIndicatorView(kIndicatorColor);
    notification_indicator_->SetPaintToLayer();
    notification_indicator_->layer()->SetFillsBoundsOpaquely(false);
    notification_indicator_->SetVisible(false);
    AddChildView(notification_indicator_);
  }

  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
}

ShelfButton::~ShelfButton() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;
}

void ShelfButton::SetShadowedImage(const gfx::ImageSkia& image) {
  icon_view_->SetImage(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      image, icon_shadows_));
}

void ShelfButton::SetImage(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // TODO: need an empty image.
    icon_view_->SetImage(image);
    return;
  }

  const int icon_size = ShelfConstants::button_icon_size();

  if (icon_size > image.width() || icon_size > image.height()) {
    LOG(WARNING) << "An icon of size " << image.width() << "x" << image.height()
                 << "is being scaled up and will look blurry.";
  }

  // Resize the image maintaining our aspect ratio.
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = icon_size;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > icon_size) {
    width = icon_size;
    height = static_cast<int>(width / aspect_ratio);
  }

  if (width == image.width() && height == image.height()) {
    SetShadowedImage(image);
    return;
  }

  SetShadowedImage(gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size(width, height)));
}

const gfx::ImageSkia& ShelfButton::GetImage() const {
  return icon_view_->GetImage();
}

void ShelfButton::AddState(State state) {
  if (!(state_ & state)) {
    state_ |= state;
    Layout();
    if (state & STATE_ATTENTION)
      indicator_->ShowAttention(true);

    if (state & STATE_ACTIVE)
      indicator_->ShowActiveStatus(true);

    if (is_notification_indicator_enabled_ && (state & STATE_NOTIFICATION))
      notification_indicator_->SetVisible(true);

    if (state & STATE_DRAGGING)
      ScaleAppIcon(true);
  }
}

void ShelfButton::ClearState(State state) {
  if (state_ & state) {
    state_ &= ~state;
    Layout();
    if (state & STATE_ATTENTION)
      indicator_->ShowAttention(false);
    if (state & STATE_ACTIVE)
      indicator_->ShowActiveStatus(false);

    if (is_notification_indicator_enabled_ && (state & STATE_NOTIFICATION))
      notification_indicator_->SetVisible(false);

    if (state & STATE_DRAGGING)
      ScaleAppIcon(false);
  }
}

gfx::Rect ShelfButton::GetIconBounds() const {
  return icon_view_->bounds();
}

views::InkDrop* ShelfButton::GetInkDropForTesting() {
  return GetInkDrop();
}

void ShelfButton::OnDragStarted(const ui::LocatedEvent* event) {
  AnimateInkDrop(views::InkDropState::HIDDEN, event);
}

void ShelfButton::OnMenuClosed() {
  DCHECK_EQ(views::InkDropState::ACTIVATED,
            GetInkDrop()->GetTargetInkDropState());
  GetInkDrop()->AnimateToState(views::InkDropState::DEACTIVATED);
}

void ShelfButton::ShowContextMenu(const gfx::Point& p,
                                  ui::MenuSourceType source_type) {
  if (!context_menu_controller())
    return;

  bool destroyed = false;
  destroyed_flag_ = &destroyed;

  Button::ShowContextMenu(p, source_type);

  if (source_type == ui::MenuSourceType::MENU_SOURCE_MOUSE ||
      source_type == ui::MenuSourceType::MENU_SOURCE_KEYBOARD) {
    GetInkDrop()->AnimateToState(views::InkDropState::ACTIVATED);
  }
  if (!destroyed) {
    destroyed_flag_ = nullptr;
    // The menu will not propagate mouse events while its shown. To address,
    // the hover state gets cleared once the menu was shown (and this was not
    // destroyed). In case context menu is shown target view does not receive
    // OnMouseReleased events and we need to cancel capture manually.
    if (shelf_view_->drag_view() == this)
      OnMouseCaptureLost();
    else
      ClearState(STATE_HOVERED);
  }
}

const char* ShelfButton::GetClassName() const {
  return kViewClassName;
}

bool ShelfButton::OnMousePressed(const ui::MouseEvent& event) {
  Button::OnMousePressed(event);

  // No need to scale up the app for mouse right click since the app can't be
  // dragged through right button.
  if (!(event.flags() & ui::EF_LEFT_MOUSE_BUTTON))
    return true;

  shelf_view_->PointerPressedOnButton(this, ShelfView::MOUSE, event);

  if (shelf_view_->IsDraggedView(this)) {
    drag_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kDragTimeThresholdMs),
        base::Bind(&ShelfButton::OnTouchDragTimer, base::Unretained(this)));
  }
  return true;
}

void ShelfButton::OnMouseReleased(const ui::MouseEvent& event) {
  Button::OnMouseReleased(event);
  drag_timer_.Stop();
  ClearState(STATE_DRAGGING);
  // PointerReleasedOnButton deletes the ShelfButton when user drags a pinned
  // running app from shelf.
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, false);
  // WARNING: we may have been deleted.
}

void ShelfButton::OnMouseCaptureLost() {
  ClearState(STATE_HOVERED);
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, true);
  Button::OnMouseCaptureLost();
}

bool ShelfButton::OnMouseDragged(const ui::MouseEvent& event) {
  Button::OnMouseDragged(event);
  shelf_view_->PointerDraggedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void ShelfButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(shelf_view_->GetTitleForView(this));
}

void ShelfButton::Layout() {
  // TODO: Find out why there is an extra pixel of padding between each item
  // and the inner side of the shelf.
  int icon_padding =
      (ShelfConstants::shelf_size() - ShelfConstants::button_icon_size()) / 2 -
      1;
  const int icon_size = ShelfConstants::button_icon_size();
  const int status_indicator_offet_from_shelf_edge =
      ShelfConstants::status_indicator_offset_from_edge();

  const gfx::Rect button_bounds(GetContentsBounds());
  Shelf* shelf = shelf_view_->shelf();
  const bool is_horizontal_shelf = shelf->IsHorizontalAlignment();
  int x_offset = is_horizontal_shelf ? 0 : icon_padding;
  int y_offset = is_horizontal_shelf ? icon_padding : 0;

  int icon_width = std::min(icon_size, button_bounds.width() - x_offset);
  int icon_height = std::min(icon_size, button_bounds.height() - y_offset);

  // If on the left or top 'invert' the inset so the constant gap is on
  // the interior (towards the center of display) edge of the shelf.
  if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
    x_offset = button_bounds.width() - (icon_size + icon_padding);

  // Center icon with respect to the secondary axis.
  if (is_horizontal_shelf)
    x_offset = std::max(0, button_bounds.width() - icon_width) / 2;
  else
    y_offset = std::max(0, button_bounds.height() - icon_height) / 2;

  // Expand bounds to include shadows.
  gfx::Insets insets_shadows = gfx::ShadowValue::GetMargin(icon_shadows_);
  // Adjust offsets to center icon, not icon + shadow.
  x_offset += (insets_shadows.left() - insets_shadows.right()) / 2;
  y_offset += (insets_shadows.top() - insets_shadows.bottom()) / 2;
  gfx::Rect icon_view_bounds =
      gfx::Rect(button_bounds.x() + x_offset, button_bounds.y() + y_offset,
                icon_width, icon_height);

  // The indicators should be aligned with the icon, not the icon + shadow.
  gfx::Point indicator_midpoint = icon_view_bounds.CenterPoint();
  if (is_notification_indicator_enabled_) {
    notification_indicator_->SetBoundsRect(
        gfx::Rect(icon_view_bounds.right() - kNotificationIndicatorRadiusDip,
                  icon_view_bounds.y(), kNotificationIndicatorRadiusDip * 2,
                  kNotificationIndicatorRadiusDip * 2));
  }

  icon_view_bounds.Inset(insets_shadows);
  icon_view_bounds.AdjustToFit(gfx::Rect(size()));
  icon_view_->SetBoundsRect(icon_view_bounds);

  // Icon size has been incorrect when running
  // PanelLayoutManagerTest.PanelAlignmentSecondDisplay on valgrind bot, see
  // http://crbug.com/234854.
  DCHECK_LE(icon_width, icon_size);
  DCHECK_LE(icon_height, icon_size);

  switch (shelf->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      indicator_midpoint.set_y(button_bounds.bottom() -
                               kStatusIndicatorRadiusDip -
                               status_indicator_offet_from_shelf_edge);
      break;
    case SHELF_ALIGNMENT_LEFT:
      indicator_midpoint.set_x(button_bounds.x() + kStatusIndicatorRadiusDip +
                               status_indicator_offet_from_shelf_edge);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      indicator_midpoint.set_x(button_bounds.right() -
                               kStatusIndicatorRadiusDip -
                               status_indicator_offet_from_shelf_edge);
      break;
  }

  gfx::Rect indicator_bounds(indicator_midpoint, gfx::Size());
  indicator_bounds.Inset(gfx::Insets(-kStatusIndicatorMaxSize));
  indicator_->SetBoundsRect(indicator_bounds);

  UpdateState();
}

void ShelfButton::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

void ShelfButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      AddState(STATE_HOVERED);
      drag_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(kDragTimeThresholdMs),
          base::Bind(&ShelfButton::OnTouchDragTimer, base::Unretained(this)));
      ripple_activation_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kInkDropRippleActivationTimeMs),
          base::Bind(&ShelfButton::OnRippleTimer, base::Unretained(this)));
      GetInkDrop()->AnimateToState(views::InkDropState::ACTION_PENDING);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      drag_timer_.Stop();
      // If the button is being dragged, or there is an active context menu,
      // for this ShelfButton, don't deactivate the ink drop.
      if (!(state_ & STATE_DRAGGING) &&
          !shelf_view_->IsShowingMenuForView(this) &&
          (GetInkDrop()->GetTargetInkDropState() ==
           views::InkDropState::ACTIVATED)) {
        GetInkDrop()->AnimateToState(views::InkDropState::DEACTIVATED);
      }
      ClearState(STATE_HOVERED);
      ClearState(STATE_DRAGGING);
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (state_ & STATE_DRAGGING) {
        shelf_view_->PointerPressedOnButton(this, ShelfView::TOUCH, *event);
        event->SetHandled();
      } else {
        // The drag went to the bezel and is about to be passed to
        // ShelfLayoutManager.
        drag_timer_.Stop();
        GetInkDrop()->AnimateToState(views::InkDropState::HIDDEN);
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if ((state_ & STATE_DRAGGING) && shelf_view_->IsDraggedView(this)) {
        shelf_view_->PointerDraggedOnButton(this, ShelfView::TOUCH, *event);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      if (state_ & STATE_DRAGGING) {
        ClearState(STATE_DRAGGING);
        shelf_view_->PointerReleasedOnButton(this, ShelfView::TOUCH, false);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_LONG_TAP:
      GetInkDrop()->AnimateToState(views::InkDropState::ACTIVATED);
      // Handle LONG_TAP to avoid opening the context menu twice.
      event->SetHandled();
      break;
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      GetInkDrop()->AnimateToState(views::InkDropState::ACTIVATED);
      break;
    default:
      break;
  }

  if (!event->handled())
    return Button::OnGestureEvent(event);
}

std::unique_ptr<views::InkDropRipple> ShelfButton::CreateInkDropRipple() const {
  const int ink_drop_small_size = ash::ShelfConstants::shelf_size();
  return std::make_unique<views::SquareInkDropRipple>(
      gfx::Size(kInkDropLargeSize, kInkDropLargeSize),
      ink_drop_large_corner_radius(),
      gfx::Size(ink_drop_small_size, ink_drop_small_size),
      ink_drop_small_corner_radius(), GetLocalBounds().CenterPoint(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

bool ShelfButton::ShouldEnterPushedState(const ui::Event& event) {
  if (!shelf_view_->ShouldEventActivateButton(this, event))
    return false;

  return Button::ShouldEnterPushedState(event);
}

std::unique_ptr<views::InkDrop> ShelfButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

void ShelfButton::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);
  if (listener_)
    listener_->ButtonPressed(this, event, GetInkDrop());
}

void ShelfButton::UpdateState() {
  const bool is_horizontal_shelf =
      shelf_view_->shelf()->IsHorizontalAlignment();

  indicator_->SetVisible(!(state_ & STATE_HIDDEN) &&
                         (state_ & STATE_ATTENTION || state_ & STATE_RUNNING ||
                          state_ & STATE_ACTIVE));
  indicator_->SetHorizontalShelf(is_horizontal_shelf);

  icon_view_->SetHorizontalAlignment(is_horizontal_shelf
                                         ? views::ImageView::CENTER
                                         : views::ImageView::LEADING);
  icon_view_->SetVerticalAlignment(is_horizontal_shelf
                                       ? views::ImageView::LEADING
                                       : views::ImageView::CENTER);
  SchedulePaint();
}

void ShelfButton::OnTouchDragTimer() {
  AddState(STATE_DRAGGING);
}

void ShelfButton::OnRippleTimer() {
  if (GetInkDrop()->GetTargetInkDropState() !=
      views::InkDropState::ACTION_PENDING) {
    return;
  }
  GetInkDrop()->AnimateToState(views::InkDropState::ACTIVATED);
}

void ShelfButton::ScaleAppIcon(bool scale_up) {
  ui::ScopedLayerAnimationSettings settings(icon_view_->layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kDragDropAppIconScaleTransitionMs));

  if (scale_up) {
    icon_view_->layer()->SetTransform(gfx::GetScaleTransform(
        gfx::Rect(icon_view_->layer()->bounds().size()).CenterPoint(),
        kAppIconScale));
  } else {
    icon_view_->layer()->SetTransform(gfx::Transform());
  }
}

}  // namespace ash
