// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider.h"

#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_util.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// The length of short side of the black bar of the divider in dp.
constexpr int kDividerShortSideLength = 8;
constexpr int kDividerEnlargedShortSideLength = 16;

// The length of the white bar of the divider in dp.
constexpr int kWhiteBarShortSideLength = 2;
constexpr int kWhiteBarLongSideLength = 16;
constexpr int kWhiteBarRadius = 4;
constexpr int kWhiteBarCornerRadius = 1;

constexpr SkColor kDividerBackgroundColor = SK_ColorBLACK;
constexpr SkColor kWhiteBarBackgroundColor = SK_ColorWHITE;
constexpr int kDividerBoundsChangeDurationMs = 250;
constexpr int kWhiteBarBoundsChangeDurationMs = 150;

// The distance to the divider edge in which a touch gesture will be considered
// as a valid event on the divider.
constexpr int kDividerEdgeInsetForTouch = 5;

// The window targeter that is installed on the always on top container window
// when the split view mode is active.
class AlwaysOnTopWindowTargeter : public aura::WindowTargeter {
 public:
  explicit AlwaysOnTopWindowTargeter(aura::Window* divider_window)
      : divider_window_(divider_window) {}
  ~AlwaysOnTopWindowTargeter() override = default;

 private:
  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override {
    if (target == divider_window_) {
      *hit_test_rect_mouse = *hit_test_rect_touch = gfx::Rect(target->bounds());
      hit_test_rect_touch->Inset(
          gfx::Insets(-kDividerEdgeInsetForTouch, -kDividerEdgeInsetForTouch));
      return true;
    }
    return aura::WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                                 hit_test_rect_touch);
  }

  aura::Window* divider_window_;

  DISALLOW_COPY_AND_ASSIGN(AlwaysOnTopWindowTargeter);
};

// The white handler bar in the middle of the divider.
class DividerHandlerView : public RoundedRectView {
 public:
  DividerHandlerView(int corner_radius, SkColor background_color)
      : RoundedRectView(corner_radius, background_color) {}
  ~DividerHandlerView() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    // It's needed to avoid artifacts when tapping on the divider quickly.
    canvas->DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
    RoundedRectView::OnPaint(canvas);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DividerHandlerView);
};

// The divider view class. Passes the mouse/gesture events to the controller.
class DividerView : public views::View,
                    public views::ViewTargeterDelegate,
                    public gfx::AnimationDelegate {
 public:
  explicit DividerView(SplitViewDivider* divider)
      : controller_(Shell::Get()->split_view_controller()),
        divider_(divider),
        white_bar_animation_(this) {
    divider_view_ = new views::View();
    divider_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    divider_view_->layer()->SetColor(kDividerBackgroundColor);

    divider_handler_view_ =
        new DividerHandlerView(kWhiteBarCornerRadius, kWhiteBarBackgroundColor);
    divider_handler_view_->SetPaintToLayer();

    AddChildView(divider_view_);
    AddChildView(divider_handler_view_);

    SetEventTargeter(
        std::unique_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
    white_bar_animation_.SetSlideDuration(kWhiteBarBoundsChangeDurationMs);
  }
  ~DividerView() override { white_bar_animation_.Stop(); }

  // views::View:
  void Layout() override {
    divider_view_->SetBoundsRect(GetLocalBounds());
    UpdateWhiteHandlerBounds();
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->StartResize(location);
    OnResizeStatusChanged();
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->Resize(location);
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->EndResize(location);
    OnResizeStatusChanged();
    if (event.GetClickCount() == 2)
      controller_->SwapWindows();
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    gfx::Point location(event->location());
    views::View::ConvertPointToScreen(this, &location);
    switch (event->type()) {
      case ui::ET_GESTURE_TAP:
        if (event->details().tap_count() == 2)
          controller_->SwapWindows();
        break;
      case ui::ET_GESTURE_TAP_DOWN:
      case ui::ET_GESTURE_SCROLL_BEGIN:
        controller_->StartResize(location);
        OnResizeStatusChanged();
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        controller_->Resize(location);
        break;
      case ui::ET_GESTURE_END:
        controller_->EndResize(location);
        OnResizeStatusChanged();
        break;
      default:
        break;
    }
    event->SetHandled();
  }

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    return true;
  }

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    UpdateWhiteHandlerBounds();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    UpdateWhiteHandlerBounds();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    UpdateWhiteHandlerBounds();
  }

 private:
  void OnResizeStatusChanged() {
    // It's possible that when this function is called, split view mode has
    // been ended, and the divider widget is to be deleted soon. In this case
    // no need to update the divider layout and do the animation.
    if (!controller_->IsSplitViewModeActive())
      return;

    // Do the white handler bar enlarge/shrink animation when starting/ending
    // dragging.
    if (controller_->is_resizing())
      white_bar_animation_.Show();
    else
      white_bar_animation_.Hide();

    // Do the divider enlarge/shrink animation when starting/ending dragging.
    divider_view_->SetBoundsRect(GetLocalBounds());
    const gfx::Rect old_bounds =
        divider_->GetDividerBoundsInScreen(/*is_dragging=*/false);
    const gfx::Rect new_bounds =
        divider_->GetDividerBoundsInScreen(controller_->is_resizing());
    gfx::Transform transform;
    transform.Translate(new_bounds.x() - old_bounds.x(),
                        new_bounds.y() - old_bounds.y());
    transform.Scale(
        static_cast<float>(new_bounds.width()) / old_bounds.width(),
        static_cast<float>(new_bounds.height()) / old_bounds.height());
    ui::ScopedLayerAnimationSettings settings(
        divider_view_->layer()->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kDividerBoundsChangeDurationMs));
    settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    divider_view_->SetTransform(transform);
  }

  // Returns the expected bounds of the white divider handler.
  void UpdateWhiteHandlerBounds() {
    // Calculate the width/height/radius for the rounded rectangle.
    int width, height, radius;
    const int expected_width_unselected =
        controller_->IsCurrentScreenOrientationLandscape()
            ? kWhiteBarShortSideLength
            : kWhiteBarLongSideLength;
    const int expected_height_unselected =
        controller_->IsCurrentScreenOrientationLandscape()
            ? kWhiteBarLongSideLength
            : kWhiteBarShortSideLength;
    if (white_bar_animation_.is_animating()) {
      width = white_bar_animation_.CurrentValueBetween(
          expected_width_unselected, kWhiteBarRadius * 2);
      height = white_bar_animation_.CurrentValueBetween(
          expected_height_unselected, kWhiteBarRadius * 2);
      radius = white_bar_animation_.CurrentValueBetween(kWhiteBarCornerRadius,
                                                        kWhiteBarRadius);
    } else {
      if (controller_->is_resizing()) {
        width = kWhiteBarRadius * 2;
        height = kWhiteBarRadius * 2;
        radius = kWhiteBarRadius;
      } else {
        width = expected_width_unselected;
        height = expected_height_unselected;
        radius = kWhiteBarCornerRadius;
      }
    }

    gfx::Rect white_bar_bounds(GetLocalBounds());
    white_bar_bounds.ClampToCenteredSize(gfx::Size(width, height));
    divider_handler_view_->SetCornerRadius(radius);
    divider_handler_view_->SetBoundsRect(white_bar_bounds);
  }

  views::View* divider_view_ = nullptr;
  DividerHandlerView* divider_handler_view_ = nullptr;
  SplitViewController* controller_;
  SplitViewDivider* divider_;
  gfx::SlideAnimation white_bar_animation_;

  DISALLOW_COPY_AND_ASSIGN(DividerView);
};

}  // namespace

SplitViewDivider::SplitViewDivider(SplitViewController* controller,
                                   aura::Window* root_window)
    : controller_(controller) {
  Shell::Get()->activation_client()->AddObserver(this);
  CreateDividerWidget(root_window);

  aura::Window* always_on_top_container =
      Shell::GetContainer(root_window, kShellWindowId_AlwaysOnTopContainer);
  split_view_window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
      always_on_top_container, std::make_unique<AlwaysOnTopWindowTargeter>(
                                   divider_widget_->GetNativeWindow()));
}

SplitViewDivider::~SplitViewDivider() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  divider_widget_->Close();
  split_view_window_targeter_.reset();
  for (auto* iter : observed_windows_)
    iter->RemoveObserver(this);
  observed_windows_.clear();
}

// static
gfx::Size SplitViewDivider::GetDividerSize(
    const gfx::Rect& work_area_bounds,
    OrientationLockType screen_orientation,
    bool is_dragging) {
  if (IsLandscapeOrientation(screen_orientation)) {
    return is_dragging
               ? gfx::Size(kDividerEnlargedShortSideLength,
                           work_area_bounds.height())
               : gfx::Size(kDividerShortSideLength, work_area_bounds.height());
  } else {
    return is_dragging
               ? gfx::Size(work_area_bounds.width(),
                           kDividerEnlargedShortSideLength)
               : gfx::Size(work_area_bounds.width(), kDividerShortSideLength);
  }
}

// static
gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(
    const gfx::Rect& work_area_bounds_in_screen,
    OrientationLockType screen_orientation,
    int divider_position,
    bool is_dragging) {
  const gfx::Size divider_size = GetDividerSize(
      work_area_bounds_in_screen, screen_orientation, is_dragging);
  int dragging_diff =
      (kDividerEnlargedShortSideLength - kDividerShortSideLength) / 2;
  switch (screen_orientation) {
    case OrientationLockType::kLandscapePrimary:
    case OrientationLockType::kLandscapeSecondary:
      return is_dragging
                 ? gfx::Rect(work_area_bounds_in_screen.x() + divider_position -
                                 dragging_diff,
                             work_area_bounds_in_screen.y(),
                             divider_size.width(), divider_size.height())
                 : gfx::Rect(work_area_bounds_in_screen.x() + divider_position,
                             work_area_bounds_in_screen.y(),
                             divider_size.width(), divider_size.height());
    case OrientationLockType::kPortraitPrimary:
    case OrientationLockType::kPortraitSecondary:
      return is_dragging
                 ? gfx::Rect(work_area_bounds_in_screen.x(),
                             work_area_bounds_in_screen.y() + divider_position -
                                 (kDividerEnlargedShortSideLength -
                                  kDividerShortSideLength) /
                                     2,
                             divider_size.width(), divider_size.height())
                 : gfx::Rect(work_area_bounds_in_screen.x(),
                             work_area_bounds_in_screen.y() + divider_position,
                             divider_size.width(), divider_size.height());
    default:
      NOTREACHED();
      return gfx::Rect();
  }
}

void SplitViewDivider::UpdateDividerBounds() {
  divider_widget_->SetBounds(GetDividerBoundsInScreen(/*is_dragging=*/false));
}

gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(bool is_dragging) {
  aura::Window* root_window =
      divider_widget_->GetNativeWindow()->GetRootWindow();
  const gfx::Rect work_area_bounds_in_screen =
      controller_->GetDisplayWorkAreaBoundsInScreen(root_window);
  const int divider_position = controller_->divider_position();
  const OrientationLockType screen_orientation =
      controller_->GetCurrentScreenOrientation();
  return GetDividerBoundsInScreen(work_area_bounds_in_screen,
                                  screen_orientation, divider_position,
                                  is_dragging);
}

void SplitViewDivider::AddObservedWindow(aura::Window* window) {
  if (!base::ContainsValue(observed_windows_, window)) {
    window->AddObserver(this);
    ::wm::TransientWindowManager::GetOrCreate(window)->AddObserver(this);
    observed_windows_.push_back(window);
  }
}

void SplitViewDivider::RemoveObservedWindow(aura::Window* window) {
  auto iter =
      std::find(observed_windows_.begin(), observed_windows_.end(), window);
  if (iter != observed_windows_.end()) {
    window->RemoveObserver(this);
    ::wm::TransientWindowManager::GetOrCreate(window)->RemoveObserver(this);
    observed_windows_.erase(iter);
  }
}

void SplitViewDivider::OnWindowDragStarted(aura::Window* dragged_window) {
  is_dragging_window_ = true;
  SetAlwaysOnTop(false);
  // Make sure |divider_widget_| is placed below the dragged window.
  dragged_window->parent()->StackChildBelow(divider_widget_->GetNativeWindow(),
                                            dragged_window);
}

void SplitViewDivider::OnWindowDragEnded() {
  is_dragging_window_ = false;
  SetAlwaysOnTop(true);
}

void SplitViewDivider::OnWindowDestroying(aura::Window* window) {
  RemoveObservedWindow(window);
}

void SplitViewDivider::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  // We only care about the bounds change of windows in
  // |transient_windows_observer_|.
  if (!transient_windows_observer_.IsObserving(window))
    return;

  // |window|'s transient parent must be one of the windows in
  // |observed_windows_|.
  aura::Window* transient_parent = nullptr;
  for (auto* observed_window : observed_windows_) {
    if (::wm::HasTransientAncestor(window, observed_window)) {
      transient_parent = observed_window;
      break;
    }
  }
  DCHECK(transient_parent);

  gfx::Rect transient_bounds = window->GetBoundsInScreen();
  transient_bounds.AdjustToFit(transient_parent->GetBoundsInScreen());
  window->SetBoundsInScreen(
      transient_bounds,
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

void SplitViewDivider::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (!is_dragging_window_ &&
      (!gained_active ||
       base::ContainsValue(observed_windows_, gained_active))) {
    SetAlwaysOnTop(true);
  } else {
    // If |gained_active| is not one of the observed windows, or there is one
    // window that is currently being dragged, |divider_widget_| should not
    // be placed on top.
    SetAlwaysOnTop(false);
  }
}

void SplitViewDivider::OnTransientChildAdded(aura::Window* window,
                                             aura::Window* transient) {
  // For now, we only care about dialog bubbles type transient child. We may
  // observe other types transient child window as well if need arises in the
  // future.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(transient);
  if (!widget || !widget->widget_delegate()->AsBubbleDialogDelegate())
    return;

  // At this moment, the transient window may not have the valid bounds yet.
  // Start observe the transient window.
  transient_windows_observer_.Add(transient);
}

void SplitViewDivider::OnTransientChildRemoved(aura::Window* window,
                                               aura::Window* transient) {
  if (transient_windows_observer_.IsObserving(transient))
    transient_windows_observer_.Remove(transient);
}

void SplitViewDivider::CreateDividerWidget(aura::Window* root_window) {
  DCHECK(!divider_widget_);
  // Native widget owns this widget.
  divider_widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_AlwaysOnTopContainer);
  DividerView* divider_view = new DividerView(this);
  divider_widget_->set_focus_on_creation(false);
  divider_widget_->Init(params);
  divider_widget_->SetContentsView(divider_view);
  divider_widget_->SetBounds(GetDividerBoundsInScreen(false /* is_dragging */));
  divider_widget_->Show();
}

void SplitViewDivider::SetAlwaysOnTop(bool on_top) {
  if (on_top) {
    divider_widget_->SetAlwaysOnTop(true);

    // Special handling when put divider into always_on_top container. We want
    // to put it at the bottom so it won't block other always_on_top windows.
    aura::Window* always_on_top_container =
        Shell::GetContainer(divider_widget_->GetNativeWindow()->GetRootWindow(),
                            kShellWindowId_AlwaysOnTopContainer);
    always_on_top_container->StackChildAtBottom(
        divider_widget_->GetNativeWindow());
  } else {
    divider_widget_->SetAlwaysOnTop(false);
  }
}

}  // namespace ash
