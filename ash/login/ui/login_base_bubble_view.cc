// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_base_bubble_view.h"

#include <memory>

#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/scoped_observer.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Total width of the bubble view.
constexpr int kBubbleTotalWidthDp = 192;

// Padding around the bubble view.
constexpr int kBubblePaddingDp = 16;

// Spacing between the child view inside the bubble view.
constexpr int kBubbleBetweenChildSpacingDp = 16;

// Border radius of the rounded bubble.
constexpr int kErrorBubbleBorderRadius = 8;

// The amount of time for bubble show/hide animation.
constexpr base::TimeDelta kBubbleAnimationDuration =
    base::TimeDelta::FromMilliseconds(300);

}  // namespace

// This class handles keyboard, mouse, and focus events, and dismisses the
// associated bubble in response.
class LoginBubbleHandler : public ui::EventHandler {
 public:
  LoginBubbleHandler(LoginBaseBubbleView* bubble) : bubble_(bubble) {
    Shell::Get()->AddPreTargetHandler(this);
  }

  ~LoginBubbleHandler() override { Shell::Get()->RemovePreTargetHandler(this); }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED)
      ProcessPressedEvent(event->AsLocatedEvent());
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP ||
        event->type() == ui::ET_GESTURE_TAP_DOWN) {
      ProcessPressedEvent(event->AsLocatedEvent());
    }
  }

  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::ET_KEY_PRESSED ||
        event->key_code() == ui::VKEY_PROCESSKEY) {
      return;
    }

    if (!bubble_->GetVisible())
      return;

    if (bubble_->GetBubbleOpener() && bubble_->GetBubbleOpener()->HasFocus())
      return;

    if (login_views_utils::HasFocusInAnyChildView(bubble_))
      return;

    if (!bubble_->IsPersistent())
      bubble_->Hide();
  }

 private:
  void ProcessPressedEvent(const ui::LocatedEvent* event) {
    if (!bubble_->GetVisible())
      return;

    gfx::Point screen_location = event->location();
    ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                               &screen_location);

    // Don't do anything with clicks inside the bubble.
    if (bubble_->GetBoundsInScreen().Contains(screen_location))
      return;

    // Let the bubble opener handle clicks on itself.
    if (bubble_->GetBubbleOpener() &&
        bubble_->GetBubbleOpener()->GetBoundsInScreen().Contains(
            screen_location)) {
      return;
    }

    if (!bubble_->IsPersistent())
      bubble_->Hide();
  }

  LoginBaseBubbleView* bubble_;

  DISALLOW_COPY_AND_ASSIGN(LoginBubbleHandler);
};

LoginBaseBubbleView::LoginBaseBubbleView(views::View* anchor_view)
    : LoginBaseBubbleView(anchor_view, nullptr) {}

LoginBaseBubbleView::LoginBaseBubbleView(views::View* anchor_view,
                                         aura::Window* parent_window)
    : anchor_view_(anchor_view),
      bubble_handler_(std::make_unique<LoginBubbleHandler>(this)) {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(kBubblePaddingDp), kBubbleBetweenChildSpacingDp));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  SetVisible(false);
}

void LoginBaseBubbleView::EnsureLayer() {
  if (layer())
    return;
  // Layer rendering is needed for animation.
  SetPaintToLayer();
  SkColor background_color = AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  layer()->SetBackgroundBlur(
      static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault));
  SetBackground(views::CreateRoundedRectBackground(background_color,
                                                   kErrorBubbleBorderRadius));
  layer()->SetFillsBoundsOpaquely(false);
}

LoginBaseBubbleView::~LoginBaseBubbleView() = default;

void LoginBaseBubbleView::Show() {
  if (layer())
    layer()->GetAnimator()->RemoveObserver(this);

  SetSize(GetPreferredSize());
  SetPosition(CalculatePosition());

  ScheduleAnimation(true /*visible*/);

  // Tell ChromeVox to read bubble contents.
  NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                           true /*send_native_event*/);
}

void LoginBaseBubbleView::Hide() {
  ScheduleAnimation(false /*visible*/);
}

LoginButton* LoginBaseBubbleView::GetBubbleOpener() const {
  return nullptr;
}

bool LoginBaseBubbleView::IsPersistent() const {
  return false;
}

void LoginBaseBubbleView::SetPersistent(bool persistent) {}

gfx::Point LoginBaseBubbleView::CalculatePosition() {
  if (GetAnchorView()) {
    gfx::Point bottom_left = GetAnchorView()->bounds().bottom_left();
    ConvertPointToTarget(GetAnchorView()->parent() /*source*/,
                         parent() /*target*/, &bottom_left);
    return bottom_left;
  }

  return gfx::Point();
}

void LoginBaseBubbleView::SetAnchorView(views::View* anchor_view) {
  anchor_view_ = anchor_view;
}

void LoginBaseBubbleView::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  layer()->GetAnimator()->RemoveObserver(this);
  SetVisible(false);
  DestroyLayer();
}

void LoginBaseBubbleView::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  // The animation for this view should never be aborted.
  NOTREACHED();
}

gfx::Size LoginBaseBubbleView::CalculatePreferredSize() const {
  gfx::Size size;
  size.set_width(kBubbleTotalWidthDp);
  size.set_height(GetHeightForWidth(kBubbleTotalWidthDp));
  return size;
}

void LoginBaseBubbleView::Layout() {
  views::View::Layout();

  // If a Layout() is called while the bubble is visible (i.e. due to Show()),
  // its bounds may change because of the parent's LayoutManager. This allows
  // the bubbles to always determine their own size and position.
  if (GetVisible()) {
    SetSize(GetPreferredSize());
    SetPosition(CalculatePosition());
  }
}

void LoginBaseBubbleView::OnBlur() {
  Hide();
}

gfx::Point LoginBaseBubbleView::CalculatePositionUsingDefaultStrategy(
    PositioningStrategy strategy,
    int horizontal_padding,
    int vertical_padding) const {
  if (!GetAnchorView())
    return gfx::Point();

  gfx::Point anchor_position = GetAnchorView()->bounds().origin();
  ConvertPointToTarget(GetAnchorView()->parent() /*source*/,
                       GetAnchorView()->GetWidget()->GetRootView() /*target*/,
                       &anchor_position);
  auto bounds = GetBoundsAvailableToShowBubble();
  gfx::Size bubble_size(width() + 2 * horizontal_padding,
                        height() + vertical_padding);
  gfx::Point result = gfx::Point();
  switch (strategy) {
    case PositioningStrategy::kShowOnLeftSideOrRightSide:
      result = login_views_utils::CalculateBubblePositionLeftRightStrategy(
          {anchor_position, GetAnchorView()->size()}, bubble_size, bounds);
      break;
    case PositioningStrategy::kShowOnRightSideOrLeftSide:
      result = login_views_utils::CalculateBubblePositionRightLeftStrategy(
          {anchor_position, GetAnchorView()->size()}, bubble_size, bounds);
      break;
  }
  // Get position of the bubble surrounded by paddings.
  result.Offset(horizontal_padding, 0);
  ConvertPointToTarget(GetAnchorView()->GetWidget()->GetRootView() /*source*/,
                       parent() /*target*/, &result);
  return result;
}

gfx::Rect LoginBaseBubbleView::GetBoundsAvailableToShowBubble() const {
  auto bounds = GetRootViewBounds();
  auto work_area = GetWorkArea();
  // Get min means here to exclude either shelf or virtual keyaboard.
  bounds.set_height(std::min(bounds.height(), work_area.height()));
  return bounds;
}

gfx::Rect LoginBaseBubbleView::GetRootViewBounds() const {
  return GetAnchorView()->GetWidget()->GetRootView()->GetLocalBounds();
}

gfx::Rect LoginBaseBubbleView::GetWorkArea() const {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(GetAnchorView()->GetWidget()->GetNativeWindow())
      .work_area();
}

void LoginBaseBubbleView::ScheduleAnimation(bool visible) {
  if (GetBubbleOpener()) {
    GetBubbleOpener()->AnimateInkDrop(visible
                                          ? views::InkDropState::ACTIVATED
                                          : views::InkDropState::DEACTIVATED,
                                      nullptr /*event*/);
  }

  if (layer())
    layer()->GetAnimator()->StopAnimating();

  EnsureLayer();
  float opacity_start = 0.0f;
  float opacity_end = 1.0f;
  if (!visible) {
    std::swap(opacity_start, opacity_end);
    // We only need to handle animation ending if we're hiding the bubble.
    layer()->GetAnimator()->AddObserver(this);
  } else {
    SetVisible(true);
  }

  layer()->SetOpacity(opacity_start);
  {
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(kBubbleAnimationDuration);
    settings.SetTweenType(visible ? gfx::Tween::EASE_OUT : gfx::Tween::EASE_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(opacity_end);
  }
}

}  // namespace ash
