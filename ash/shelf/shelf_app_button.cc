// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/shelf/shelf_app_button.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dot_indicator.h"
#include "ash/style/style_util.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/window_util.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_shortcut_image.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace {

constexpr int kStatusIndicatorRadiusDip = 2;
constexpr int kStatusIndicatorMaxSize = 10;
constexpr int kStatusIndicatorActiveSizeJellyEnabled = 12;
constexpr int kStatusIndicatorRunningSizeJellyEnabled = 6;
constexpr int kStatusIndicatorThickness = 2;

// The size of the notification indicator circle over the size of the icon.
constexpr float kNotificationIndicatorWidthRatio = 14.0f / 64.0f;

// The size of the notification indicator circle padding over the size of the
// icon.
constexpr float kNotificationIndicatorPaddingRatio = 5.0f / 64.0f;

constexpr SkColor kDefaultIndicatorColor = SK_ColorWHITE;
constexpr SkAlpha kInactiveIndicatorOpacity = 0x80;

// The time threshold before an item can be dragged.
constexpr int kDragTimeThresholdMs = 300;

// The time threshold before the ink drop should activate on a long press.
constexpr int kInkDropRippleActivationTimeMs = 650;

// The drag and drop app icon should get scaled by this factor.
constexpr float kAppIconScale = 1.2f;

// The preferred promise icon size for a placeholder icon. Placeholder icons do
// not change size between states.
constexpr int kPlaceholderIconDimension = 24;

// The preferred promise icon size if the app is currently installing.
constexpr int kPromiseIconDimensionInstalling = 28;

// The preferred promise icon size if the app is currently pending.
constexpr int kPromiseIconDimensionPending = 24;

// The width of the promise app progress ring.
constexpr int kPromiseRingStrokeSize = 2;

// The amount of space between the progress ring and the promise app background
// and icon.
constexpr gfx::Insets kProgressRingMarginInstalling = gfx::Insets(-1);
constexpr gfx::Insets kProgressRingMarginPending = gfx::Insets(-2);

// The drag and drop app icon scaling up or down animation transition duration.
constexpr int kDragDropAppIconScaleTransitionMs = 200;

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

  ShelfAppButtonAnimation(const ShelfAppButtonAnimation&) = delete;
  ShelfAppButtonAnimation& operator=(const ShelfAppButtonAnimation&) = delete;

  static ShelfAppButtonAnimation* GetInstance() {
    static ShelfAppButtonAnimation* s_instance = new ShelfAppButtonAnimation();
    return s_instance;
  }

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
    if (observers_.empty())
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
    animation_.SetThrobDuration(base::Milliseconds(800));
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
};

// Draws a circular background for a promise icon view.
class PromiseIconBackground : public views::Background {
 public:
  PromiseIconBackground(ui::ColorId color_id,
                        const gfx::Rect& icon_bounds,
                        const gfx::Insets& insets)
      : color_id_(color_id), icon_bounds_(icon_bounds), insets_(insets) {}

  PromiseIconBackground(const PromiseIconBackground&) = delete;
  PromiseIconBackground& operator=(const PromiseIconBackground&) = delete;
  ~PromiseIconBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect bounds = icon_bounds_;
    bounds.Inset(insets_);

    const float radius =
        std::min(bounds.size().width(), bounds.size().height()) / 2.f;

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(get_color());

    canvas->DrawCircle(bounds.CenterPoint(), radius, flags);
  }

  void OnViewThemeChanged(views::View* view) override {
    SetNativeControlColor(view->GetColorProvider()->GetColor(color_id_));
    view->SchedulePaint();
  }

 private:
  const ui::ColorId color_id_;
  const gfx::Rect icon_bounds_;
  const gfx::Insets insets_;
};

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ShelfAppButton::AppStatusIndicatorView

class ShelfAppButton::AppStatusIndicatorView
    : public gfx::AnimationDelegate,
      public views::View,
      public ShelfAppButtonAnimation::Observer {
  METADATA_HEADER(AppStatusIndicatorView, views::View)

 public:
  AppStatusIndicatorView() {
    // Make sure the events reach the parent view for handling.
    SetCanProcessEventsWithinSubtree(false);
    status_change_animation_ = std::make_unique<gfx::SlideAnimation>(this);
    status_change_animation_->SetSlideDuration(base::Milliseconds(250));
    status_change_animation_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  }

  AppStatusIndicatorView(const AppStatusIndicatorView&) = delete;
  AppStatusIndicatorView& operator=(const AppStatusIndicatorView&) = delete;

  ~AppStatusIndicatorView() override {
    ShelfAppButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    if (!GetColorProvider()) {
      return;
    }

    gfx::ScopedCanvas scoped(canvas);

    const float dsf = canvas->UndoDeviceScaleFactor();
    gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
    cc::PaintFlags flags;
    flags.setColor(GetJellyColor());

    // Active and running indicators look a little different in the new UI.
    flags.setAntiAlias(true);
    flags.setStrokeCap(cc::PaintFlags::Cap::kRound_Cap);
    flags.setStrokeJoin(cc::PaintFlags::Join::kRound_Join);
    flags.setStrokeWidth(kStatusIndicatorThickness);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    float stroke_length = GetStrokeLength();
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

  float GetStrokeLength() {
    if (status_change_animation_->is_animating()) {
      return status_change_animation_->CurrentValueBetween(
          kStatusIndicatorRunningSizeJellyEnabled,
          kStatusIndicatorActiveSizeJellyEnabled);
    }

    return active_ ? kStatusIndicatorActiveSizeJellyEnabled
                   : kStatusIndicatorRunningSizeJellyEnabled;
  }

  SkColor GetJellyColor() {
    const SkColor active_color =
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface);
    const SkColor inactive_color =
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSecondary);
    if (show_attention_) {
      if (!ShelfAppButtonAnimation::GetInstance()->HasObserver(this)) {
        return active_color;
      }
      return SkColorSetA(active_color,
                         ShelfAppButtonAnimation::GetInstance()->GetAlpha());
    }

    if (status_change_animation_->is_animating()) {
      return gfx::Tween::ColorValueBetween(
          status_change_animation_->GetCurrentValue(), inactive_color,
          active_color);
    }
    return active_ ? active_color : inactive_color;
  }

  SkAlpha GetAlpha() {
    if (show_attention_) {
      return ShelfAppButtonAnimation::GetInstance()->HasObserver(this)
                 ? ShelfAppButtonAnimation::GetInstance()->GetAlpha()
                 : SK_AlphaOPAQUE;
    }

    if (status_change_animation_->is_animating()) {
      return status_change_animation_->CurrentValueBetween(
          kInactiveIndicatorOpacity, SK_AlphaOPAQUE);
    }

    return active_ ? SK_AlphaOPAQUE : kInactiveIndicatorOpacity;
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

    if (status_change_animation_->is_animating())
      status_change_animation_->End();

    if (show_attention_) {
      animation_end_time_ = base::TimeTicks::Now() + base::Seconds(10);
      ShelfAppButtonAnimation::GetInstance()->AddObserver(this);
    } else {
      ShelfAppButtonAnimation::GetInstance()->RemoveObserver(this);
    }
  }

  // gfx::AnimationDelegate
  void AnimationProgressed(const gfx::Animation* animation) override {
    if (animation != status_change_animation_.get())
      return;
    SchedulePaint();
  }

  void ShowActiveStatus(bool active) {
    if (active_ == active)
      return;
    active_ = active;
    if (active_)
      status_change_animation_->Show();
    else
      status_change_animation_->Hide();
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
  std::unique_ptr<gfx::SlideAnimation> status_change_animation_;
  base::TimeTicks animation_end_time_;  // For attention throbbing underline.
};

BEGIN_METADATA(ShelfAppButton, AppStatusIndicatorView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// ShelfAppButton

// static
bool ShelfAppButton::ShouldHandleEventFromContextMenu(
    const ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureEnd:
    case ui::EventType::kGestureTapCancel:
    case ui::EventType::kGestureScrollBegin:
    case ui::EventType::kGestureScrollUpdate:
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart:
      return true;
    default:
      return false;
  }
}

ShelfAppButton::ShelfAppButton(ShelfView* shelf_view,
                               ShelfButtonDelegate* shelf_button_delegate)
    : ShelfButton(shelf_view->shelf(), shelf_button_delegate),
      shelf_view_(shelf_view),
      indicator_(new AppStatusIndicatorView()) {
  const gfx::ShadowValue kShadows[] = {
      gfx::ShadowValue(gfx::Vector2d(0, 2), 0, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 3), 1, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 0), 1, SkColorSetARGB(0x54, 0, 0, 0)),
  };
  icon_shadows_.assign(kShadows, kShadows + std::size(kShadows));

  views::InkDrop::Get(this)->SetMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  views::InkDrop::UseInkDropForSquareRipple(views::InkDrop::Get(this),
                                            /*highlight_on_hover=*/false);

  views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](ShelfAppButton* host) -> std::unique_ptr<views::InkDropRipple> {
        const gfx::Rect small_ripple_area = host->CalculateSmallRippleArea();
        const int ripple_size = host->shelf_view_->GetShelfItemRippleSize();

        auto* const ink_drop = views::InkDrop::Get(host);
        const SkColor base_color = ink_drop->GetBaseColor();
        const float base_alpha = SkColorGetA(base_color);
        return std::make_unique<views::SquareInkDropRipple>(
            ink_drop, gfx::Size(ripple_size, ripple_size),
            ink_drop->GetLargeCornerRadius(), small_ripple_area.size(),
            ink_drop->GetSmallCornerRadius(), small_ripple_area.CenterPoint(),
            SkColorSetA(base_color, SK_AlphaOPAQUE),
            (base_alpha / SK_AlphaOPAQUE) * ink_drop->GetVisibleOpacity());
      },
      this));

  // TODO: refactor the layers so each button doesn't require 3.
  // |icon_view_| needs its own layer so it can be scaled up independently of
  // the ink drop ripple.
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());

  icon_view_->SetPaintToLayer();
  icon_view_->layer()->SetFillsBoundsOpaquely(false);
  icon_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  icon_view_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  // Do not make this interactive, so that events are sent to ShelfView.
  icon_view_->SetCanProcessEventsWithinSubtree(false);

  indicator_->SetPaintToLayer();
  indicator_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(indicator_.get());

  notification_indicator_ =
      AddChildView(std::make_unique<DotIndicator>(kDefaultIndicatorColor));

  views::InkDrop::Get(this)->GetInkDrop()->AddObserver(this);

  // Do not set a clip, allow the ink drop to burst out.
  views::InstallEmptyHighlightPathGenerator(this);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
  // The focus ring should have an inset of half the focus border thickness, so
  // the parent view won't clip it.
  views::FocusRing::Get(this)->SetPathGenerator(
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets::VH(views::FocusRing::kDefaultHaloThickness / 2, 0), 0));

  UpdateAccessibleDescription();
  UpdateAccessibleName();
}

ShelfAppButton::~ShelfAppButton() {
  views::InkDrop::Get(this)->GetInkDrop()->RemoveObserver(this);
}

void ShelfAppButton::SetShadowedImage(const gfx::ImageSkia& image) {
  icon_view_->SetImage(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      image, icon_shadows_));
}

void ShelfAppButton::UpdateMainAndMaybeHostBadgeIconImage() {
  if (is_promise_app_ || progress_indicator_ || has_host_badge_) {
    icon_view_->SetImage(GetIconImage(icon_scale_));
    return;
  }

  SetShadowedImage(GetIconImage(icon_scale_));
}

gfx::ImageSkia ShelfAppButton::GetImage() const {
  return icon_view_->GetImage();
}

gfx::ImageSkia ShelfAppButton::GetIconImage(float icon_scale) const {
  gfx::ImageSkia icon_image;

  bool use_fallback_icon =
      force_fallback_icon_ || !fallback_icon_image_model_.IsEmpty();

  const ui::ImageModel& image_model =
      use_fallback_icon ? fallback_icon_image_model_ : icon_image_model_;

  auto* color_provider = GetColorProvider();
  if (!color_provider) {
    color_provider = shelf_view_->GetColorProvider();
  }
  if (image_model.IsImage()) {
    icon_image = image_model.GetImage().AsImageSkia();
  } else if (image_model.IsVectorIcon()) {
    icon_image = ui::ThemedVectorIcon(image_model.GetVectorIcon())
                     .GetImageSkia(color_provider);
  }
  const gfx::Size preferred_size =
      GetPreferredIconSize(image_model, icon_scale);
  if (icon_image.size() != preferred_size) {
    icon_image = gfx::ImageSkiaOperations::CreateResizedImage(
        icon_image, skia::ImageOperations::RESIZE_BEST, preferred_size);
  }

  if (has_host_badge_ && GetColorProvider()) {
    const int main_icon_radius = std::round(
        icon_scale * shelf_view_->GetShelfShortcutIconContainerSize() / 2.0f);
    const int badge_radius = std::round(
        icon_scale * shelf_view_->GetShelfShortcutHostBadgeContainerSize() /
        2.0f);
    const int teardrop_corner_radius = std::round(
        icon_scale * shelf_view_->GetShelfShortcutTeardropCornerRadiusSize());
    const int badge_icon_size = std::round(
        icon_scale * shelf_view_->GetShelfShortcutHostBadgeIconSize());

    return apps::AppShortcutImage::CreateImageWithBadgeAndTeardropBackground(
        main_icon_radius, teardrop_corner_radius, badge_radius,
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBaseOpaque),
        icon_image,
        gfx::ImageSkiaOperations::CreateResizedImage(
            host_badge_image_, skia::ImageOperations::RESIZE_BEST,
            gfx::Size(badge_icon_size, badge_icon_size)));
  }
  return icon_image;
}

gfx::ImageSkia ShelfAppButton::GetBadgeIconImage(float icon_scale) const {
  if (host_badge_image_.isNull()) {
    return gfx::ImageSkia();
  }

  const int background_radius =
      std::round(icon_scale *
                 shelf_view_->GetShelfShortcutHostBadgeContainerSize() / 2.0f);
  const int icon_size =
      std::round(icon_scale * shelf_view_->GetShelfShortcutHostBadgeIconSize());
  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      background_radius,
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBaseOpaque),
      gfx::ImageSkiaOperations::CreateResizedImage(
          host_badge_image_, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(icon_size, icon_size)));
}

void ShelfAppButton::SetMainAndMaybeHostBadgeImage(
    const gfx::ImageSkia& main_image,
    bool has_placeholder_icon,
    const gfx::ImageSkia& host_badge_image) {
  has_icon_image_ = !main_image.isNull() && !has_placeholder_icon;
  has_host_badge_ = !host_badge_image.isNull();

  if (has_icon_image_ && !force_fallback_icon_) {
    fallback_icon_image_model_ = ui::ImageModel();
    if (!progress_indicator_) {
      // Clear background set as a result of adding progress indicator.
      SetBackground(nullptr);
    }
    if (has_host_badge_) {
      host_badge_image_ = host_badge_image;
    }
  }

  if (is_promise_app_ && has_placeholder_icon) {
    icon_image_model_ = ui::ImageModel(ui::ImageModel::FromVectorIcon(
        ash::kPlaceholderAppIcon, cros_tokens::kCrosSysPrimary));
  } else {
    icon_image_model_ =
        ui::ImageModel(ui::ImageModel::FromImageSkia(main_image));
  }

  UpdateMainAndMaybeHostBadgeIconImage();
}

void ShelfAppButton::AddState(State state) {
  if (!(state_ & state)) {
    state_ |= state;
    InvalidateLayout();
    if (state & STATE_ATTENTION)
      indicator_->ShowAttention(true);

    if (state & STATE_ACTIVE)
      indicator_->ShowActiveStatus(true);

    if (state & STATE_NOTIFICATION)
      notification_indicator_->SetVisible(true);

    if (state & STATE_DRAGGING)
      ScaleAppIcon(true);
  }
}

void ShelfAppButton::ClearState(State state) {
  if (state_ & state) {
    state_ &= ~state;
    DeprecatedLayoutImmediately();
    if (state & STATE_ATTENTION)
      indicator_->ShowAttention(false);
    if (state & STATE_ACTIVE)
      indicator_->ShowActiveStatus(false);

    if (state & STATE_NOTIFICATION)
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

gfx::Rect ShelfAppButton::GetIdealIconBounds(const gfx::Size& button_size,
                                             float icon_scale) const {
  return GetIconViewBounds(gfx::Rect(button_size), icon_scale,
                           /*ignore_shadow_insets=*/false);
}

views::InkDrop* ShelfAppButton::GetInkDropForTesting() {
  return views::InkDrop::Get(this)->GetInkDrop();
}

void ShelfAppButton::OnDragStarted(const ui::LocatedEvent* event) {
  views::InkDrop::Get(this)->AnimateToState(views::InkDropState::HIDDEN, event);
}

void ShelfAppButton::OnMenuClosed() {
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::DEACTIVATED);
  context_menu_target_visibility_ = false;
}

void ShelfAppButton::ShowContextMenu(const gfx::Point& p,
                                     ui::MenuSourceType source_type) {
  // Return early if:
  // 1. the context menu controller is not set; or
  // 2. `context_menu_target_visibility_` is already true.
  if (!context_menu_controller() || context_menu_target_visibility_)
    return;

  context_menu_target_visibility_ = true;
  auto weak_this = weak_factory_.GetWeakPtr();

  ShelfButton::ShowContextMenu(p, source_type);

  // This object may have been destroyed by ShowContextMenu.
  if (weak_this) {
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

bool ShelfAppButton::ShouldEnterPushedState(const ui::Event& event) {
  if (!shelf_view_->ShouldEventActivateButton(this, event))
    return false;

  return Button::ShouldEnterPushedState(event);
}

void ShelfAppButton::ReflectItemStatus(const ShelfItem& item) {
  if (item.has_notification)
    AddState(ShelfAppButton::STATE_NOTIFICATION);
  else
    ClearState(ShelfAppButton::STATE_NOTIFICATION);

  is_promise_app_ = item.is_promise_app;

  package_id_ = item.package_id;

  // Progress is incremental always by server side implementation. Do not use
  // equal for comparing progress as float point errors may surface.
  if (progress_ < item.progress || app_status_ != item.app_status) {
    progress_ = item.progress;
    app_status_ = item.app_status;
    UpdateProgressRingBounds();
    UpdateAccessibleDescription();
  }

  const ShelfID active_id = shelf_view_->model()->active_shelf_id();
  if (!active_id.IsNull() && item.id == active_id) {
    // The active status trumps all other statuses.
    AddState(ShelfAppButton::STATE_ACTIVE);
    ClearState(ShelfAppButton::STATE_RUNNING);
    ClearState(ShelfAppButton::STATE_ATTENTION);

    // Notify the parent scrollable shelf view to show the current active app.
    shelf_button_delegate()->OnAppButtonActivated(this);
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

void ShelfAppButton::AnimateInFromPromiseApp(
    const ui::ImageModel& fallback_icon,
    const base::RepeatingClosure& callback) {
  forced_progress_indicator_value_ = 0.9999f;
  UpdateProgressRingBounds();

  force_fallback_icon_ = true;
  fallback_icon_image_model_ = fallback_icon;
  UpdateMainAndMaybeHostBadgeIconImage();

  gfx::Rect icon_bounds(GetIconViewBounds(GetContentsBounds(), 1.0f,
                                          /*ignore_shadow_insets=*/false));
  const float starting_size =
      fallback_icon_image_model_.IsVectorIcon()
          ? kPlaceholderIconDimension
          : static_cast<float>(kPromiseIconDimensionInstalling);
  // TODO(b/297866814): Shadow insets are ignored for promise apps when
  // calculating icon bounds - make `GetIconViewBounds()` explicitly ignore
  // them.
  gfx::Rect promise_icon_bounds = GetIconViewBounds(
      GetContentsBounds(),
      starting_size / static_cast<float>(shelf_view_->GetButtonIconSize()),
      /*ignore_shadow_insets=*/true);
  icon_view_->layer()->SetTransform(gfx::TransformBetweenRects(
      gfx::RectF(icon_bounds), gfx::RectF(promise_icon_bounds)));

  // Animate the app list view out of the promise app state.
  views::AnimationBuilder animation;
  animation.OnEnded(base::BindOnce(&ShelfAppButton::OnAnimatedInFromPromiseApp,
                                   weak_factory_.GetWeakPtr(), callback));
  animation.OnAborted(
      base::BindOnce(&ShelfAppButton::OnAnimatedInFromPromiseApp,
                     weak_factory_.GetWeakPtr(), callback));
  animation.Once()
      .SetDuration(base::Milliseconds(100))
      .SetOpacity(progress_indicator_->layer(), 0.0f,
                  gfx::Tween::FAST_OUT_LINEAR_IN)
      .SetTransform(icon_view_->layer(), gfx::Transform(),
                    gfx::Tween::FAST_OUT_LINEAR_IN);
}

void ShelfAppButton::OnContextMenuModelRequestCanceled() {
  // The request for the context menu model gets canceled so reset the context
  // menu target visibility.
  context_menu_target_visibility_ = false;
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
  if (display::Screen::GetScreen()->InTabletMode() && padding > 0) {
    // Note that `current_index` may be nullopt while the button is fading out
    // after it's been removed from the model - for example, see
    // https://crbug.com/1355561.
    const std::optional<size_t> current_index =
        shelf_view_->view_model()->GetIndexOfView(this);
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

bool ShelfAppButton::OnMousePressed(const ui::MouseEvent& event) {
  // Clear any closing desks so that the user does not try to interact with an
  // app that is open on a closing desk.
  DesksController::Get()->MaybeCommitPendingDeskRemoval();

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
    drag_timer_.Start(FROM_HERE, base::Milliseconds(kDragTimeThresholdMs),
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

bool ShelfAppButton::ImageModelHasPlaceholderIcon() const {
  bool use_fallback_icon =
      force_fallback_icon_ || !fallback_icon_image_model_.IsEmpty();
  return use_fallback_icon ? fallback_icon_image_model_.IsVectorIcon()
                           : icon_image_model_.IsVectorIcon();
}

float ShelfAppButton::GetIconDimensionByAppState() const {
  if (is_promise_app_ && features::ArePromiseIconsEnabled()) {
    if (ImageModelHasPlaceholderIcon()) {
      return kPromiseIconDimensionPending;
    }

    switch (app_status_) {
      case AppStatus::kPending:
        return kPromiseIconDimensionPending;
      case AppStatus::kInstalling:
      case AppStatus::kInstallCancelled:
      case AppStatus::kInstallSuccess:
      case AppStatus::kPaused:
        return kPromiseIconDimensionInstalling;
      case AppStatus::kReady:
      case AppStatus::kBlocked:
        return shelf_view_->GetButtonIconSize();
    }
  }

  return shelf_view_->GetButtonIconSize();
}

gfx::Rect ShelfAppButton::GetIconViewBounds(const gfx::Rect& button_bounds,
                                            float icon_scale,
                                            bool ignore_shadow_insets) const {
  const float icon_size =
      (ImageModelHasPlaceholderIcon() ? kPlaceholderIconDimension
                                      : GetIconDimensionByAppState()) *
      icon_scale;
  const float icon_padding = (shelf_view_->GetButtonSize() - icon_size) / 2;

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
  // TODO(b/297866814): Promise icon calculation looks off because of the shadow
  // insets. To get a centered icon within the ring, we removed insets for
  // shadows. Consider improving the calculation on UpdateProgressRingBounds()
  // to account for the shadows as well.
  gfx::Insets insets_shadows =
      (is_promise_app_ || has_host_badge_ || ignore_shadow_insets)
          ? gfx::Insets()
          : gfx::ShadowValue::GetMargin(icon_shadows_);
  // Center icon with respect to the secondary axis.
  if (is_horizontal_shelf) {
    x_offset = std::max(0.0f, button_bounds.width() - icon_width) / 2;
  } else {
    y_offset = std::max(0.0f, button_bounds.height() - icon_height) / 2;
  }
  gfx::RectF icon_view_bounds =
      gfx::RectF(button_bounds.x() + x_offset, button_bounds.y() + y_offset,
                 icon_width, icon_height);

  icon_view_bounds.Inset(gfx::InsetsF(insets_shadows));
  // Icon size has been incorrect when running
  // PanelLayoutManagerTest.PanelAlignmentSecondDisplay on valgrind bot, see
  // http://crbug.com/234854.
  DCHECK_LE(icon_width, icon_size);
  DCHECK_LE(icon_height, icon_size);
  return gfx::ToRoundedRect(icon_view_bounds);
}

gfx::Rect ShelfAppButton::GetNotificationIndicatorBounds(float icon_scale) {
  gfx::Rect scaled_icon_view_bounds = GetIconViewBounds(
      GetContentsBounds(), icon_scale, /*ignore_shadow_bounds=*/false);
  float diameter =
      kNotificationIndicatorWidthRatio * scaled_icon_view_bounds.width();
  float padding =
      kNotificationIndicatorPaddingRatio * scaled_icon_view_bounds.width();
  return gfx::ToRoundedRect(
      gfx::RectF(scaled_icon_view_bounds.right() - diameter - padding,
                 scaled_icon_view_bounds.y() + padding, diameter, diameter));
}

void ShelfAppButton::Layout(PassKey) {
  Shelf* shelf = shelf_view_->shelf();
  gfx::Rect icon_view_bounds =
      GetIconViewBounds(GetContentsBounds(), icon_scale_,
                        /*ignore_shadow_bounds=*/false);
  const gfx::Rect button_bounds(GetContentsBounds());
  const int status_indicator_offet_from_shelf_edge =
      ShelfConfig::Get()->status_indicator_offset_from_shelf_edge();
  icon_view_->SetBoundsRect(icon_view_bounds);

  notification_indicator_->SetIndicatorBounds(
      GetNotificationIndicatorBounds(icon_scale_));

  // The indicators should be aligned with the icon, not the icon + shadow.
  // Use 1.0 as icon scale for |indicator_midpoint|, otherwise integer rounding
  // can incorrectly move the midpoint.
  gfx::Point indicator_midpoint =
      GetIconViewBounds(GetContentsBounds(), 1.0,
                        /*ignore_shadow_bounds=*/false)
          .CenterPoint();
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
  views::FocusRing::Get(this)->DeprecatedLayoutImmediately();
  UpdateProgressRingBounds();
}

void ShelfAppButton::ChildPreferredSizeChanged(views::View* child) {
  DeprecatedLayoutImmediately();
}

void ShelfAppButton::OnThemeChanged() {
  ShelfButton::OnThemeChanged();

  UpdateMainAndMaybeHostBadgeIconImage();

  // Redraw progress indicator to adjust colors.
  if (progress_indicator_) {
    progress_indicator_->InvalidateLayer();
  }
}

void ShelfAppButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTapDown:
      if (shelf_view_->shelf()->IsVisible()) {
        AddState(STATE_HOVERED);
        drag_timer_.Start(FROM_HERE, base::Milliseconds(kDragTimeThresholdMs),
                          base::BindOnce(&ShelfAppButton::OnTouchDragTimer,
                                         base::Unretained(this)));
        ripple_activation_timer_.Start(
            FROM_HERE, base::Milliseconds(kInkDropRippleActivationTimeMs),
            base::BindOnce(&ShelfAppButton::OnRippleTimer,
                           base::Unretained(this)));
        views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
            views::InkDropState::ACTION_PENDING);
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureTap:
      [[fallthrough]];  // Ensure tapped items are not enlarged for drag.
    case ui::EventType::kGestureEnd:
      // If the button is being dragged, or there is an active context menu,
      // for this ShelfAppButton, don't deactivate the ink drop.
      if (!(state_ & STATE_DRAGGING) &&
          !shelf_view_->IsShowingMenuForView(this) &&
          (views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() ==
           views::InkDropState::ACTIVATED)) {
        views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
            views::InkDropState::DEACTIVATED);
      } else if (event->type() == ui::EventType::kGestureEnd) {
        // When the gesture ends, we may need to deactivate the button's
        // inkdrop. For example, when a mouse event interputs the gesture press
        // on a shelf app button, the button's inkdrop could be in the pending
        // state while the button's context menu is hidden. In this case, we
        // have to hide the inkdrop explicitly.
        MaybeHideInkDropWhenGestureEnds();
      }

      ClearDragStateOnGestureEnd();
      break;
    case ui::EventType::kGestureScrollBegin:
      if (state_ & STATE_DRAGGING) {
        shelf_view_->PointerPressedOnButton(this, ShelfView::TOUCH, *event);
        event->SetHandled();
      } else {
        // The drag went to the bezel and is about to be passed to
        // ShelfLayoutManager.
        drag_timer_.Stop();
        views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
            views::InkDropState::HIDDEN);
      }
      break;
    case ui::EventType::kGestureScrollUpdate:
      if ((state_ & STATE_DRAGGING) && shelf_view_->IsDraggedView(this)) {
        shelf_view_->PointerDraggedOnButton(this, ShelfView::TOUCH, *event);
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart:
      if (state_ & STATE_DRAGGING) {
        ClearState(STATE_DRAGGING);
        shelf_view_->PointerReleasedOnButton(this, ShelfView::TOUCH, false);
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureLongTap:
      views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
          views::InkDropState::ACTIVATED);

      // The context menu may not show (for example, a mouse click which occurs
      // before the end of gesture could close the context menu). In this case,
      // let the overridden function handles the event to show the context menu
      // (see https://crbug.com/1126491).
      if (shelf_view_->IsShowingMenu()) {
        // Handle LONG_TAP to avoid opening the context menu twice.
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureTwoFingerTap:
      views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
          views::InkDropState::ACTIVATED);
      break;
    default:
      break;
  }

  if (!event->handled())
    return Button::OnGestureEvent(event);
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
  UpdateAccessibleName();
  SchedulePaint();
}

void ShelfAppButton::OnTouchDragTimer() {
  AddState(STATE_DRAGGING);
}

void ShelfAppButton::OnRippleTimer() {
  if (views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() !=
      views::InkDropState::ACTION_PENDING) {
    return;
  }
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::ACTIVATED);
}

gfx::Transform ShelfAppButton::GetScaleTransform(float icon_scale) {
  gfx::RectF pre_scaling_bounds(GetMirroredRect(GetIconViewBounds(
      GetContentsBounds(), 1.0f, /*ignore_shadow_bounds=*/false)));
  gfx::RectF target_bounds(GetMirroredRect(GetIconViewBounds(
      GetContentsBounds(), icon_scale, /*ignore_shadow_bounds=*/false)));
  return gfx::TransformBetweenRects(target_bounds, pre_scaling_bounds);
}

gfx::Size ShelfAppButton::GetPreferredIconSize(
    const ui::ImageModel& image_model,
    float icon_scale) const {
  // Placeholder icons do not change base size between states.
  if (image_model.IsVectorIcon() && !has_host_badge_) {
    return gfx::Size(kPlaceholderIconDimension * icon_scale,
                     kPlaceholderIconDimension * icon_scale);
  }

  const int icon_size = has_host_badge_
                            ? shelf_view_->GetShortcutIconSize() * icon_scale
                            : GetIconDimensionByAppState() * icon_scale;

  const gfx::Size current_icon_size = image_model.Size();

  // Resize the image maintaining our aspect ratio.
  float aspect_ratio = static_cast<float>(current_icon_size.width()) /
                       static_cast<float>(current_icon_size.height());
  int height = icon_size;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > icon_size) {
    width = icon_size;
    height = static_cast<int>(width / aspect_ratio);
  }

  return gfx::Size(width, height);
}

void ShelfAppButton::ScaleAppIcon(bool scale_up) {
  StopObservingImplicitAnimations();

  if (scale_up) {
    icon_scale_ = kAppIconScale;
    UpdateMainAndMaybeHostBadgeIconImage();
    icon_view_->layer()->SetTransform(GetScaleTransform(kAppIconScale));
  }
  ui::ScopedLayerAnimationSettings settings(icon_view_->layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::Milliseconds(kDragDropAppIconScaleTransitionMs));
  if (scale_up) {
    icon_view_->layer()->SetTransform(gfx::Transform());
  } else {
    // To avoid poor quality icons, update icon image with the correct scale
    // after the transform animation is completed.
    settings.AddObserver(this);
    icon_view_->layer()->SetTransform(GetScaleTransform(kAppIconScale));
  }

  // Animate the notification indicator alongside the |icon_view_|.
  if (notification_indicator_) {
    gfx::RectF pre_scale(GetMirroredRect(GetNotificationIndicatorBounds(1.0)));
    gfx::RectF post_scale(
        GetMirroredRect(GetNotificationIndicatorBounds(kAppIconScale)));
    gfx::Transform scale_transform =
        gfx::TransformBetweenRects(post_scale, pre_scale);

    if (scale_up)
      notification_indicator_->layer()->SetTransform(scale_transform);
    ui::ScopedLayerAnimationSettings notification_settings(
        notification_indicator_->layer()->GetAnimator());
    notification_settings.SetTransitionDuration(
        base::Milliseconds(kDragDropAppIconScaleTransitionMs));
    notification_indicator_->layer()->SetTransform(scale_up ? gfx::Transform()
                                                            : scale_transform);
  }
}

void ShelfAppButton::OnImplicitAnimationsCompleted() {
  icon_scale_ = 1.0f;
  UpdateMainAndMaybeHostBadgeIconImage();
  icon_view_->layer()->SetTransform(gfx::Transform());
  if (notification_indicator_) {
    notification_indicator_->layer()->SetTransform(gfx::Transform());
  }
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

void ShelfAppButton::SetNotificationBadgeColor(SkColor color) {
  if (notification_indicator_)
    notification_indicator_->SetColor(color);
}

void ShelfAppButton::MaybeHideInkDropWhenGestureEnds() {
  if (context_menu_target_visibility_ ||
      views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() ==
          views::InkDropState::HIDDEN) {
    // Return early if the shelf app button's context menu should show or
    // the button's inkdrop has already been hidden.
    return;
  }

  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::HIDDEN);
}

void ShelfAppButton::UpdateProgressRingBounds() {
  if ((!is_promise_app_ && !forced_progress_indicator_value_) ||
      !features::ArePromiseIconsEnabled()) {
    return;
  }

  if (!progress_indicator_) {
    progress_indicator_ =
        ProgressIndicator::CreateDefaultInstance(base::BindRepeating(
            [](ShelfAppButton* view) -> std::optional<float> {
              if (view->forced_progress_indicator_value_) {
                return *view->forced_progress_indicator_value_;
              }

              if (view->app_status() == AppStatus::kPending) {
                return 0.0f;
              }
              // If download is in-progress, return the progress as a decimal.
              // Otherwise, the progress indicator shouldn't be painted.
              float progress = view->progress();
              return (progress >= 0.f && progress < 1.f)
                         ? progress
                         : ProgressIndicator::kProgressComplete;
            },
            base::Unretained(this)));
    progress_indicator_->SetInnerIconVisible(false);
    progress_indicator_->SetInnerRingVisible(false);
    progress_indicator_->SetOuterRingStrokeWidth(
        static_cast<float>(kPromiseRingStrokeSize));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->Add(progress_indicator_->CreateLayer(base::BindRepeating(
        [](ShelfAppButton* view, ui::ColorId color_id) {
          return view->GetColorProvider()->GetColor(color_id);
        },
        base::Unretained(this))));
  }

  if (app_status() == AppStatus::kPending) {
    progress_indicator_->SetColorId(cros_tokens::kCrosSysHighlightShape);
    progress_indicator_->SetOuterRingTrackVisible(true);
  } else {
    progress_indicator_->SetColorId(cros_tokens::kCrosSysPrimary);
    progress_indicator_->SetOuterRingTrackVisible(false);
  }

  gfx::Rect progress_indicator_bounds = views::View::ConvertRectToTarget(
      icon_view_, this, icon_view_->GetImageBounds());

  const int promise_icon_preferred_dimension = GetIconDimensionByAppState();

  // If the icon is smaller than the expected icon size (e,g for placeholder
  // icons), add padding to ensure the overall size of the promise icon is
  // correct regardless of the image icon size.
  progress_indicator_bounds.Outset(
      gfx::Outsets::VH(std::max(0, (promise_icon_preferred_dimension -
                                    progress_indicator_bounds.width()) /
                                       2),
                       std::max(0, (promise_icon_preferred_dimension -
                                    progress_indicator_bounds.height()) /
                                       2)));

  const gfx::Insets progress_ring_padding =
      ImageModelHasPlaceholderIcon() || app_status() == AppStatus::kPending
          ? kProgressRingMarginPending
          : kProgressRingMarginInstalling;

  progress_indicator_bounds.Inset(progress_ring_padding);

  // The Progress indicator paints the ring within the bounds of the layer, so
  // add padding for the promise ring.
  progress_indicator_bounds.Inset(-gfx::Insets(kPromiseRingStrokeSize));

  // The masked icons include 1px padding.
  progress_indicator_bounds.Inset(1);

  progress_indicator_->layer()->SetBounds(progress_indicator_bounds);
  layer()->StackAtBottom(progress_indicator_->layer());
  progress_indicator_->InvalidateLayer();

  SetBackground(std::make_unique<PromiseIconBackground>(
      cros_tokens::kCrosSysSystemOnBase, progress_indicator_bounds,
      progress_ring_padding));
}

ProgressIndicator* ShelfAppButton::GetProgressIndicatorForTest() const {
  return progress_indicator_.get();
}

void ShelfAppButton::OnAnimatedInFromPromiseApp(
    base::RepeatingClosure callback) {
  forced_progress_indicator_value_.reset();
  if (progress_indicator_) {
    layer()->Remove(progress_indicator_->layer());
  }
  progress_indicator_.reset();
  force_fallback_icon_ = false;

  if (has_icon_image_) {
    fallback_icon_image_model_ = ui::ImageModel();
    // Clear background set as a result of adding progress indicator.
    SetBackground(nullptr);
  }
  UpdateMainAndMaybeHostBadgeIconImage();

  callback.Run();
}

void ShelfAppButton::UpdateAccessibleDescription() {
  switch (app_status_) {
    case AppStatus::kBlocked:
      GetViewAccessibility().SetDescription(
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_SHELF_ITEM_BLOCKED_APP));
      break;
    case AppStatus::kPaused:
      GetViewAccessibility().SetDescription(
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_SHELF_ITEM_PAUSED_APP));
      break;
    default:
      GetViewAccessibility().RemoveDescription();
      break;
  }
}

void ShelfAppButton::UpdateAccessibleName() {
  std::u16string accessible_name = GetViewAccessibility().GetCachedName();

  if (!accessible_name.empty()) {
    GetViewAccessibility().SetName(accessible_name);
  } else {
    accessible_name = shelf_view_->GetTitleForView(this);

    if (!accessible_name.empty()) {
      GetViewAccessibility().SetName(accessible_name);
    } else {
      GetViewAccessibility().SetName(
          std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    }
  }
}

BEGIN_METADATA(ShelfAppButton)
END_METADATA

}  // namespace ash
