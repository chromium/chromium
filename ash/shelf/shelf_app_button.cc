// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_app_button.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_view.h"
#include "ash/style/default_color_constants.h"
#include "ash/style/default_colors.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_switches.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"

namespace {

constexpr int kStatusIndicatorRadiusDip = 2;
constexpr int kStatusIndicatorMaxSize = 10;
constexpr int kStatusIndicatorActiveSize = 8;
constexpr int kStatusIndicatorRunningSize = 4;
constexpr int kStatusIndicatorThickness = 2;

constexpr int kNotificationIndicatorRadiusDip = 6;
constexpr int kNotificationIndicatorPadding = 1;

constexpr SkColor kDefaultIndicatorColor = SK_ColorWHITE;

// The time threshold before an item can be dragged.
constexpr int kDragTimeThresholdMs = 300;

// The time threshold before the ink drop should activate on a long press.
constexpr int kInkDropRippleActivationTimeMs = 650;

// The drag and drop app icon should get scaled by this factor.
constexpr float kAppIconScale = 1.2f;

// The drag and drop app icon scaling up or down animation transition duration.
constexpr int kDragDropAppIconScaleTransitionMs = 200;

// Uses the icon image to calculate the light vibrant color to be used for
// the notification indicator.
base::Optional<SkColor> CalculateNotificationColor(gfx::ImageSkia image) {
  const SkBitmap* source = image.bitmap();
  if (!source || source->empty() || source->isNull())
    return base::nullopt;

  std::vector<color_utils::ColorProfile> color_profiles;
  color_profiles.push_back(color_utils::ColorProfile(
      color_utils::LumaRange::LIGHT, color_utils::SaturationRange::VIBRANT));

  std::vector<color_utils::Swatch> best_swatches =
      color_utils::CalculateProminentColorsOfBitmap(
          *source, color_profiles, nullptr /* bitmap region */,
          color_utils::ColorSwatchFilter());

  // If the best swatch color is transparent, then
  // CalculateProminentColorsOfBitmap() failed to find a suitable color.
  if (best_swatches.empty() || best_swatches[0].color == SK_ColorTRANSPARENT)
    return base::nullopt;

  return best_swatches[0].color;
}

// Simple AnimationDelegate that owns a single ThrobAnimation instance to
// keep all Draw Attention animations in sync.
class ShelfAppButtonAnimation : public gfx::AnimationDelegate {
 public:
  class Observer {
   public:
    virtual void AnimationProgressed() = 0;

   protected:
    virtual ~Observer() = default;
  };

  static ShelfAppButtonAnimation* GetInstance() {
    static ShelfAppButtonAnimation* s_instance = new ShelfAppButtonAnimation();
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
  ShelfAppButtonAnimation() : animation_(this) {
    animation_.SetThrobDuration(base::TimeDelta::FromMilliseconds(800));
    animation_.SetTweenType(gfx::Tween::SMOOTH_IN_OUT);
  }

  ~ShelfAppButtonAnimation() override = default;

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

  DISALLOW_COPY_AND_ASSIGN(ShelfAppButtonAnimation);
};

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ShelfAppButton::AppNotificationIndicatorView

// The indicator which is activated when the app corresponding with this
// ShelfAppButton receives a notification.
class ShelfAppButton::AppNotificationIndicatorView : public views::View {
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

  void SetColor(SkColor new_color) {
    indicator_color_ = new_color;
    SchedulePaint();
  }

  SkColor GetColorForTest() { return indicator_color_; }

 private:
  SkColor indicator_color_;

  DISALLOW_COPY_AND_ASSIGN(AppNotificationIndicatorView);
};

////////////////////////////////////////////////////////////////////////////////
// ShelfAppButton::AppStatusIndicatorView

class ShelfAppButton::AppStatusIndicatorView
    : public views::View,
      public ShelfAppButtonAnimation::Observer {
 public:
  AppStatusIndicatorView() : show_attention_(false), active_(false) {
    // Make sure the events reach the parent view for handling.
    SetCanProcessEventsWithinSubtree(false);
  }

  ~AppStatusIndicatorView() override {
    ShelfAppButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped(canvas);
    if (show_attention_) {
      const SkAlpha alpha =
          ShelfAppButtonAnimation::GetInstance()->HasObserver(this)
              ? ShelfAppButtonAnimation::GetInstance()->GetAlpha()
              : SK_AlphaOPAQUE;
      canvas->SaveLayerAlpha(alpha);
    }

    const float dsf = canvas->UndoDeviceScaleFactor();
    gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
    cc::PaintFlags flags;
    // Active and running indicators look a little different in the new UI.
    flags.setColor(DeprecatedGetAppStateIndicatorColor(
        active_, kIndicatorColorActive, kInicatorColorRunning));
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
    SkPath path;
    path.moveTo(start.x() * dsf, start.y() * dsf);
    path.lineTo(end.x() * dsf, end.y() * dsf);
    canvas->DrawPath(path, flags);
  }

  // ShelfAppButtonAnimation::Observer
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
          base::TimeTicks::Now() + base::TimeDelta::FromSeconds(10);
      ShelfAppButtonAnimation::GetInstance()->AddObserver(this);
    } else {
      ShelfAppButtonAnimation::GetInstance()->RemoveObserver(this);
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
      ShelfAppButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  bool show_attention_ = false;
  bool active_ = false;
  bool horizontal_shelf_ = true;
  base::TimeTicks animation_end_time_;  // For attention throbbing underline.

  DISALLOW_COPY_AND_ASSIGN(AppStatusIndicatorView);
};

////////////////////////////////////////////////////////////////////////////////
// ShelfAppButton

// static
const char ShelfAppButton::kViewClassName[] = "ash/ShelfAppButton";

// static
bool ShelfAppButton::ShouldHandleEventFromContextMenu(
    const ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_END:
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      return true;
    default:
      return false;
  }
}

ShelfAppButton::ShelfAppButton(ShelfView* shelf_view,
                               ShelfButtonDelegate* shelf_button_delegate)
    : ShelfButton(shelf_view->shelf(), shelf_button_delegate),
      icon_view_(new views::ImageView()),
      shelf_view_(shelf_view),
      indicator_(new AppStatusIndicatorView()),
      notification_indicator_(nullptr),
      state_(STATE_NORMAL),
      destroyed_flag_(nullptr),
      is_notification_indicator_enabled_(
          features::IsNotificationIndicatorEnabled()) {
  const gfx::ShadowValue kShadows[] = {
      gfx::ShadowValue(gfx::Vector2d(0, 2), 0, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 3), 1, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 0), 1, SkColorSetARGB(0x54, 0, 0, 0)),
  };
  icon_shadows_.assign(kShadows, kShadows + base::size(kShadows));

  // TODO: refactor the layers so each button doesn't require 3.
  // |icon_view_| needs its own layer so it can be scaled up independently of
  // the ink drop ripple.
  icon_view_->SetPaintToLayer();
  icon_view_->layer()->SetFillsBoundsOpaquely(false);
  icon_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  icon_view_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  // Do not make this interactive, so that events are sent to ShelfView.
  icon_view_->SetCanProcessEventsWithinSubtree(false);

  indicator_->SetPaintToLayer();
  indicator_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(indicator_);
  AddChildView(icon_view_);
  if (is_notification_indicator_enabled_) {
    notification_indicator_ =
        new AppNotificationIndicatorView(kDefaultIndicatorColor);
    notification_indicator_->SetPaintToLayer();
    notification_indicator_->layer()->SetFillsBoundsOpaquely(false);
    notification_indicator_->SetVisible(false);
    AddChildView(notification_indicator_);
  }
  GetInkDrop()->AddObserver(this);

  // Do not set a clip, allow the ink drop to burst out.
  views::InstallEmptyHighlightPathGenerator(this);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
  // The focus ring should have an inset of half the focus border thickness, so
  // the parent view won't clip it.
  focus_ring()->SetPathGenerator(
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(views::PlatformStyle::kFocusHaloThickness / 2, 0), 0));
}

ShelfAppButton::~ShelfAppButton() {
  GetInkDrop()->RemoveObserver(this);
  if (destroyed_flag_)
    *destroyed_flag_ = true;
}

void ShelfAppButton::SetShadowedImage(const gfx::ImageSkia& image) {
  icon_view_->SetImage(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      image, icon_shadows_));
}

void ShelfAppButton::SetImage(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // TODO: need an empty image.
    icon_view_->SetImage(image);
    icon_image_ = gfx::ImageSkia();
    return;
  }
  icon_image_ = image;

  if (is_notification_indicator_enabled_) {
    base::Optional<SkColor> notification_color =
        CalculateNotificationColor(icon_image_);
    notification_indicator_->SetColor(
        notification_color.value_or(kDefaultIndicatorColor));
  }

  const int icon_size = shelf_view_->GetButtonIconSize() * icon_scale_;

  // Resize the image maintaining our aspect ratio.
  float aspect_ratio = static_cast<float>(icon_image_.width()) /
                       static_cast<float>(icon_image_.height());
  int height = icon_size;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > icon_size) {
    width = icon_size;
    height = static_cast<int>(width / aspect_ratio);
  }

  const gfx::Size preferred_size(width, height);

  if (image.size() == preferred_size) {
    SetShadowedImage(image);
    return;
  }

  SetShadowedImage(gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, preferred_size));
}

const gfx::ImageSkia& ShelfAppButton::GetImage() const {
  return icon_view_->GetImage();
}

void ShelfAppButton::AddState(State state) {
  if (!(state_ & state)) {
    state_ |= state;
    InvalidateLayout();
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

void ShelfAppButton::ClearState(State state) {
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

void ShelfAppButton::ClearDragStateOnGestureEnd() {
  drag_timer_.Stop();
  ClearState(STATE_HOVERED);
  ClearState(STATE_DRAGGING);
}

gfx::Rect ShelfAppButton::GetIconBounds() const {
  return icon_view_->bounds();
}

gfx::Rect ShelfAppButton::GetIconBoundsInScreen() const {
  return icon_view_->GetBoundsInScreen();
}

views::InkDrop* ShelfAppButton::GetInkDropForTesting() {
  return GetInkDrop();
}

void ShelfAppButton::OnDragStarted(const ui::LocatedEvent* event) {
  AnimateInkDrop(views::InkDropState::HIDDEN, event);
}

void ShelfAppButton::OnMenuClosed() {
  DCHECK_EQ(views::InkDropState::ACTIVATED,
            GetInkDrop()->GetTargetInkDropState());
  GetInkDrop()->AnimateToState(views::InkDropState::DEACTIVATED);
}

void ShelfAppButton::ShowContextMenu(const gfx::Point& p,
                                     ui::MenuSourceType source_type) {
  if (!context_menu_controller())
    return;

  bool destroyed = false;
  destroyed_flag_ = &destroyed;

  if (source_type == ui::MenuSourceType::MENU_SOURCE_MOUSE ||
      source_type == ui::MenuSourceType::MENU_SOURCE_KEYBOARD) {
    GetInkDrop()->AnimateToState(views::InkDropState::ACTIVATED);
  }

  ShelfButton::ShowContextMenu(p, source_type);

  if (!destroyed) {
    destroyed_flag_ = nullptr;
    // The menu will not propagate mouse events while it's shown. To address,
    // the hover state gets cleared once the menu was shown (and this was not
    // destroyed). In case context menu is shown target view does not receive
    // OnMouseReleased events and we need to cancel capture manually.
    if (shelf_view_->IsDraggedView(this))
      OnMouseCaptureLost();
    else
      ClearState(STATE_HOVERED);
  }
}

void ShelfAppButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ShelfButton::GetAccessibleNodeData(node_data);
  const base::string16 title = shelf_view_->GetTitleForView(this);
  node_data->SetName(title.empty() ? GetAccessibleName() : title);
}

bool ShelfAppButton::ShouldEnterPushedState(const ui::Event& event) {
  if (!shelf_view_->ShouldEventActivateButton(this, event))
    return false;

  return Button::ShouldEnterPushedState(event);
}

void ShelfAppButton::ReflectItemStatus(const ShelfItem& item) {
  if (features::IsNotificationIndicatorEnabled()) {
    if (item.has_notification)
      AddState(ShelfAppButton::STATE_NOTIFICATION);
    else
      ClearState(ShelfAppButton::STATE_NOTIFICATION);
  }

  const ShelfID active_id = shelf_view_->model()->active_shelf_id();
  if (!active_id.IsNull() && item.id == active_id) {
    // The active status trumps all other statuses.
    AddState(ShelfAppButton::STATE_ACTIVE);
    ClearState(ShelfAppButton::STATE_RUNNING);
    ClearState(ShelfAppButton::STATE_ATTENTION);
    return;
  }

  ClearState(ShelfAppButton::STATE_ACTIVE);

  switch (item.status) {
    case STATUS_CLOSED:
      ClearState(ShelfAppButton::STATE_RUNNING);
      ClearState(ShelfAppButton::STATE_ATTENTION);
      break;
    case STATUS_RUNNING:
      AddState(ShelfAppButton::STATE_RUNNING);
      ClearState(ShelfAppButton::STATE_ATTENTION);
      break;
    case STATUS_ATTENTION:
      ClearState(ShelfAppButton::STATE_RUNNING);
      AddState(ShelfAppButton::STATE_ATTENTION);
      break;
  }
}

bool ShelfAppButton::IsIconSizeCurrent() {
  gfx::Insets insets_shadows = gfx::ShadowValue::GetMargin(icon_shadows_);
  int icon_width =
      GetIconBounds().width() + insets_shadows.left() + insets_shadows.right();

  return icon_width == shelf_view_->GetButtonIconSize();
}

bool ShelfAppButton::FireDragTimerForTest() {
  if (!drag_timer_.IsRunning())
    return false;
  drag_timer_.FireNow();
  return true;
}

void ShelfAppButton::FireRippleActivationTimerForTest() {
  ripple_activation_timer_.FireNow();
}

gfx::Rect ShelfAppButton::CalculateSmallRippleArea() const {
  int ink_drop_small_size = shelf_view_->GetButtonSize();
  gfx::Point center_point = GetLocalBounds().CenterPoint();
  const int padding = ShelfConfig::Get()->GetAppIconEndPadding();

  // Add padding to the ink drop for the left-most and right-most app buttons in
  // the shelf when there is a non-zero padding between the app icon and the
  // end of scrollable shelf.
  if (TabletModeController::Get()->InTabletMode() && padding > 0) {
    const int current_index = shelf_view_->view_model()->GetIndexOfView(this);
    int left_padding =
        (shelf_view_->visible_views_indices().front() == current_index)
            ? padding
            : 0;
    int right_padding =
        (shelf_view_->visible_views_indices().back() == current_index) ? padding
                                                                       : 0;

    if (base::i18n::IsRTL())
      std::swap(left_padding, right_padding);

    ink_drop_small_size += left_padding + right_padding;

    const int x_offset = (-left_padding / 2) + (right_padding / 2);
    center_point.Offset(x_offset, 0);
  }

  gfx::Rect small_ripple_area(
      gfx::Size(ink_drop_small_size, ink_drop_small_size));
  small_ripple_area.Offset(center_point.x() - ink_drop_small_size / 2,
                           center_point.y() - ink_drop_small_size / 2);
  return small_ripple_area;
}

const char* ShelfAppButton::GetClassName() const {
  return kViewClassName;
}

bool ShelfAppButton::OnMousePressed(const ui::MouseEvent& event) {
  // TODO: This call should probably live somewhere else (such as inside
  // |ShelfView.PointerPressedOnButton|.
  // No need to scale up the app for mouse right click since the app can't be
  // dragged through right button.
  if (!(event.flags() & ui::EF_LEFT_MOUSE_BUTTON)) {
    Button::OnMousePressed(event);
    return true;
  }

  ShelfButton::OnMousePressed(event);
  shelf_view_->PointerPressedOnButton(this, ShelfView::MOUSE, event);

  if (shelf_view_->IsDraggedView(this)) {
    drag_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(kDragTimeThresholdMs),
                      base::BindOnce(&ShelfAppButton::OnTouchDragTimer,
                                     base::Unretained(this)));
  }
  return true;
}

void ShelfAppButton::OnMouseReleased(const ui::MouseEvent& event) {
  drag_timer_.Stop();
  ClearState(STATE_DRAGGING);
  ShelfButton::OnMouseReleased(event);
  // PointerReleasedOnButton deletes the ShelfAppButton when user drags a pinned
  // running app from shelf.
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, false);
  // WARNING: we may have been deleted.
}

void ShelfAppButton::OnMouseCaptureLost() {
  ClearState(STATE_HOVERED);
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, true);
  ShelfButton::OnMouseCaptureLost();
}

bool ShelfAppButton::OnMouseDragged(const ui::MouseEvent& event) {
  ShelfButton::OnMouseDragged(event);
  shelf_view_->PointerDraggedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

gfx::Rect ShelfAppButton::GetIconViewBounds(float icon_scale) {
  const float icon_size = shelf_view_->GetButtonIconSize() * icon_scale;
  const float icon_padding = (shelf_view_->GetButtonSize() - icon_size) / 2;

  const gfx::Rect button_bounds(GetContentsBounds());
  const Shelf* shelf = shelf_view_->shelf();
  const bool is_horizontal_shelf = shelf->IsHorizontalAlignment();
  float x_offset = is_horizontal_shelf ? 0 : icon_padding;
  float y_offset = is_horizontal_shelf ? icon_padding : 0;

  const float icon_width =
      std::min(icon_size, button_bounds.width() - x_offset);
  const float icon_height =
      std::min(icon_size, button_bounds.height() - y_offset);

  // If on the left or top 'invert' the inset so the constant gap is on
  // the interior (towards the center of display) edge of the shelf.
  if (ShelfAlignment::kLeft == shelf->alignment())
    x_offset = button_bounds.width() - (icon_size + icon_padding);

  // Expand bounds to include shadows.
  gfx::Insets insets_shadows = gfx::ShadowValue::GetMargin(icon_shadows_);
  // insets_shadows = insets_shadows.Scale(icon_scale);
  // Center icon with respect to the secondary axis.
  if (is_horizontal_shelf)
    x_offset = std::max(0.0f, button_bounds.width() - icon_width + 1) / 2;
  else
    y_offset = std::max(0.0f, button_bounds.height() - icon_height) / 2;
  gfx::RectF icon_view_bounds =
      gfx::RectF(button_bounds.x() + x_offset, button_bounds.y() + y_offset,
                 icon_width, icon_height);

  icon_view_bounds.Inset(insets_shadows);
  // Icon size has been incorrect when running
  // PanelLayoutManagerTest.PanelAlignmentSecondDisplay on valgrind bot, see
  // http://crbug.com/234854.
  DCHECK_LE(icon_width, icon_size);
  DCHECK_LE(icon_height, icon_size);
  return gfx::ToRoundedRect(icon_view_bounds);
}

void ShelfAppButton::Layout() {
  Shelf* shelf = shelf_view_->shelf();
  gfx::Rect icon_view_bounds = GetIconViewBounds(icon_scale_);
  const gfx::Rect button_bounds(GetContentsBounds());
  const int status_indicator_offet_from_shelf_edge =
      ShelfConfig::Get()->status_indicator_offset_from_shelf_edge();

  icon_view_->SetBoundsRect(icon_view_bounds);

  // The indicators should be aligned with the icon, not the icon + shadow.
  gfx::Point indicator_midpoint = icon_view_bounds.CenterPoint();
  if (is_notification_indicator_enabled_) {
    notification_indicator_->SetBoundsRect(gfx::Rect(
        icon_view_bounds.right() - 2 * kNotificationIndicatorRadiusDip -
            kNotificationIndicatorPadding,
        icon_view_bounds.y() + kNotificationIndicatorPadding,
        kNotificationIndicatorRadiusDip * 2,
        kNotificationIndicatorRadiusDip * 2));
  }

  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      indicator_midpoint.set_y(button_bounds.bottom() -
                               kStatusIndicatorRadiusDip -
                               status_indicator_offet_from_shelf_edge);
      break;
    case ShelfAlignment::kLeft:
      indicator_midpoint.set_x(button_bounds.x() + kStatusIndicatorRadiusDip +
                               status_indicator_offet_from_shelf_edge);
      break;
    case ShelfAlignment::kRight:
      indicator_midpoint.set_x(button_bounds.right() -
                               kStatusIndicatorRadiusDip -
                               status_indicator_offet_from_shelf_edge);
      break;
  }

  gfx::Rect indicator_bounds(indicator_midpoint, gfx::Size());
  indicator_bounds.Inset(gfx::Insets(-kStatusIndicatorMaxSize));
  indicator_->SetBoundsRect(indicator_bounds);

  UpdateState();
  focus_ring()->Layout();
}

void ShelfAppButton::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

void ShelfAppButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      if (shelf_view_->shelf()->IsVisible()) {
        AddState(STATE_HOVERED);
        drag_timer_.Start(
            FROM_HERE, base::TimeDelta::FromMilliseconds(kDragTimeThresholdMs),
            base::BindOnce(&ShelfAppButton::OnTouchDragTimer,
                           base::Unretained(this)));
        ripple_activation_timer_.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(kInkDropRippleActivationTimeMs),
            base::BindOnce(&ShelfAppButton::OnRippleTimer,
                           base::Unretained(this)));
        GetInkDrop()->AnimateToState(views::InkDropState::ACTION_PENDING);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_TAP:
      FALLTHROUGH;  // Ensure tapped items are not enlarged for drag.
    case ui::ET_GESTURE_END:
      // If the button is being dragged, or there is an active context menu,
      // for this ShelfAppButton, don't deactivate the ink drop.
      if (!(state_ & STATE_DRAGGING) &&
          !shelf_view_->IsShowingMenuForView(this) &&
          (GetInkDrop()->GetTargetInkDropState() ==
           views::InkDropState::ACTIVATED)) {
        GetInkDrop()->AnimateToState(views::InkDropState::DEACTIVATED);
      }
      ClearDragStateOnGestureEnd();
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

std::unique_ptr<views::InkDropRipple> ShelfAppButton::CreateInkDropRipple()
    const {
  const gfx::Rect small_ripple_area = CalculateSmallRippleArea();
  const int ripple_size = shelf_view_->GetShelfItemRippleSize();

  return std::make_unique<views::SquareInkDropRipple>(
      gfx::Size(ripple_size, ripple_size), GetInkDropLargeCornerRadius(),
      small_ripple_area.size(), GetInkDropSmallCornerRadius(),
      small_ripple_area.CenterPoint(), GetInkDropBaseColor(),
      GetInkDropVisibleOpacity());
}

bool ShelfAppButton::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (notification_indicator_ && notification_indicator_->GetVisible())
    shelf_view_->AnnounceShelfItemNotificationBadge(this);

  if (action_data.action == ax::mojom::Action::kScrollToMakeVisible)
    shelf_button_delegate()->HandleAccessibleActionScrollToMakeVisible(this);

  return views::View::HandleAccessibleAction(action_data);
}

void ShelfAppButton::InkDropAnimationStarted() {
  SetInkDropAnimationStarted(/*started=*/true);
}

void ShelfAppButton::InkDropRippleAnimationEnded(views::InkDropState state) {
  // Notify the host view of the ink drop to be hidden at the end of ink drop
  // animation.
  if (state == views::InkDropState::HIDDEN)
    SetInkDropAnimationStarted(/*started=*/false);
}

void ShelfAppButton::UpdateState() {
  indicator_->SetVisible(!(state_ & STATE_HIDDEN) &&
                         (state_ & STATE_ATTENTION || state_ & STATE_RUNNING ||
                          state_ & STATE_ACTIVE));

  const bool is_horizontal_shelf =
      shelf_view_->shelf()->IsHorizontalAlignment();
  indicator_->SetHorizontalShelf(is_horizontal_shelf);

  icon_view_->SetHorizontalAlignment(
      is_horizontal_shelf ? views::ImageView::Alignment::kCenter
                          : views::ImageView::Alignment::kLeading);
  icon_view_->SetVerticalAlignment(is_horizontal_shelf
                                       ? views::ImageView::Alignment::kLeading
                                       : views::ImageView::Alignment::kCenter);
  SchedulePaint();
}

void ShelfAppButton::OnTouchDragTimer() {
  AddState(STATE_DRAGGING);
}

void ShelfAppButton::OnRippleTimer() {
  if (GetInkDrop()->GetTargetInkDropState() !=
      views::InkDropState::ACTION_PENDING) {
    return;
  }
  GetInkDrop()->AnimateToState(views::InkDropState::ACTIVATED);
}

gfx::Transform ShelfAppButton::GetScaleTransform(float icon_scale) {
  gfx::RectF pre_scaling_bounds(GetIconViewBounds(1.0f));
  gfx::RectF target_bounds(GetIconViewBounds(icon_scale));
  return gfx::TransformBetweenRects(target_bounds, pre_scaling_bounds);
}

void ShelfAppButton::ScaleAppIcon(bool scale_up) {
  StopObservingImplicitAnimations();

  if (scale_up) {
    icon_scale_ = kAppIconScale;
    SetImage(icon_image_);
    icon_view_->layer()->SetTransform(GetScaleTransform(kAppIconScale));
  }
  ui::ScopedLayerAnimationSettings settings(icon_view_->layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kDragDropAppIconScaleTransitionMs));
  if (scale_up) {
    icon_view_->layer()->SetTransform(gfx::Transform());
  } else {
    // To avoid poor quality icons, update icon image with the correct scale
    // after the transform animation is completed.
    settings.AddObserver(this);
    icon_view_->layer()->SetTransform(GetScaleTransform(kAppIconScale));
  }
}

void ShelfAppButton::OnImplicitAnimationsCompleted() {
  icon_scale_ = 1.0f;
  SetImage(icon_image_);
  icon_view_->layer()->SetTransform(gfx::Transform());
}

void ShelfAppButton::SetInkDropAnimationStarted(bool started) {
  if (ink_drop_animation_started_ == started)
    return;

  ink_drop_animation_started_ = started;
  if (started) {
    ink_drop_count_ = shelf_button_delegate()->CreateScopedActiveInkDropCount(
        /*sender=*/this);
  } else {
    ink_drop_count_.reset(nullptr);
  }
}

SkColor ShelfAppButton::GetNotificationIndicatorColorForTest() {
  return notification_indicator_->GetColorForTest();
}

}  // namespace ash
