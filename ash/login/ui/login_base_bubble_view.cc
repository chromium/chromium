// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_base_bubble_view.h"

#include <memory>

#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/system_shadow.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
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
constexpr int kBubbleBorderRadius = 8;

// The amount of time for bubble show/hide animation.
constexpr base::TimeDelta kBubbleAnimationDuration = base::Milliseconds(300);

}  // namespace

// This class handles keyboard, mouse, and focus events, and dismisses the
// associated bubble in response.
class LoginBubbleHandler : public ui::EventHandler {
 public:
  explicit LoginBubbleHandler(LoginBaseBubbleView* bubble) : bubble_(bubble) {
    Shell::Get()->AddPreTargetHandler(this);
  }

  LoginBubbleHandler(const LoginBubbleHandler&) = delete;
  LoginBubbleHandler& operator=(const LoginBubbleHandler&) = delete;

  ~LoginBubbleHandler() override { Shell::Get()->RemovePreTargetHandler(this); }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      ProcessPressedEvent(event->AsLocatedEvent());
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::EventType::kGestureTap ||
        event->type() == ui::EventType::kGestureTapDown) {
      ProcessPressedEvent(event->AsLocatedEvent());
    }
  }

  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::EventType::kKeyPressed ||
        event->key_code() == ui::VKEY_PROCESSKEY) {
      return;
    }

    if (!bubble_->GetVisible()) {
      return;
    }

    // Hide the bubble if the bubble opener is about to lose focus from tab
    // traversal.
    if (bubble_->GetBubbleOpener() && bubble_->GetBubbleOpener()->HasFocus() &&
        event->key_code() != ui::VKEY_TAB) {
      return;
    }

    if (login_views_utils::HasFocusInAnyChildView(bubble_)) {
      return;
    }

    views::View* anchor = bubble_->GetAnchorView();
    if (anchor && login_views_utils::HasFocusInAnyChildView(anchor)) {
      return;
    }

    if (!bubble_->is_persistent()) {
      bubble_->Hide();
    }
  }

 private:
  void ProcessPressedEvent(const ui::LocatedEvent* event) {
    if (!bubble_->GetVisible()) {
      return;
    }

    gfx::Point screen_location = event->location();
    ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                               &screen_location);

    // Don't do anything with clicks inside the bubble.
    if (bubble_->GetBoundsInScreen().Contains(screen_location)) {
      return;
    }

    // Let the bubble opener handle clicks on itself.
    if (bubble_->GetBubbleOpener() &&
        bubble_->GetBubbleOpener()->GetBoundsInScreen().Contains(
            screen_location)) {
      return;
    }

    if (!bubble_->is_persistent()) {
      bubble_->Hide();
    }
  }

  raw_ptr<LoginBaseBubbleView> bubble_;
};

LoginBaseBubbleView::LoginBaseBubbleView(base::WeakPtr<views::View> anchor_view)
    : LoginBaseBubbleView(std::move(anchor_view), nullptr) {}

LoginBaseBubbleView::LoginBaseBubbleView(base::WeakPtr<views::View> anchor_view,
                                         aura::Window* parent_window)
    : anchor_view_(std::move(anchor_view)),
      bubble_handler_(std::make_unique<LoginBubbleHandler>(this)) {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(kBubblePaddingDp), kBubbleBetweenChildSpacingDp));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  ui::ColorId background_color_id =
      chromeos::features::IsJellyrollEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated)
          : kColorAshShieldAndBase80;

  SetBackground(views::CreateThemedRoundedRectBackground(background_color_id,
                                                         kBubbleBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kBubbleBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  // Set shadow
  if (chromeos::features::IsJellyrollEnabled()) {
    shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
        this, SystemShadow::Type::kElevation12);
    shadow_->SetRoundedCornerRadius(kBubbleBorderRadius);
  }
  SetVisible(false);
}

void LoginBaseBubbleView::EnsureLayer() {
  if (layer()) {
    return;
  }
  // Layer rendering is needed for animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
}

LoginBaseBubbleView::~LoginBaseBubbleView() = default;

void LoginBaseBubbleView::Show() {
  if (layer()) {
    layer()->GetAnimator()->RemoveObserver(this);
  }

  SetSize(GetPreferredSize());
  SetPosition(CalculatePosition());

  ScheduleAnimation(true /*visible*/);

  // Tell ChromeVox to read bubble contents.
  if (notify_a11y_alert_on_show_) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                             true /*send_native_event*/);
  }
}

void LoginBaseBubbleView::Hide() {
  ScheduleAnimation(false /*visible*/);
}

LoginButton* LoginBaseBubbleView::GetBubbleOpener() const {
  return nullptr;
}

gfx::Point LoginBaseBubbleView::CalculatePosition() {
  if (!GetAnchorView()) {
    return gfx::Point();
  }

  // Views' positions are defined in the parents' coordinate system. Therefore,
  // the position of the bubble needs to be returned in its parent's coordinate
  // system. kTryBeforeThenAfter and kTryAfterThenBefore use strategies implying
  // to know the bounds of the entire window; therefore, resulting position is
  // calculated by using the root view's coordinates system. kShowAbove and
  // kShowBelow are less complicated, they only use the coordinate system of the
  // the anchor view's parent.

  // In RTL case, we are doing mirroring in `ConvertPointToTarget` when finding
  // the position of the bubble. However, there's no need to mirror since the
  // coordinate system is from right to left and the origin is the right upper
  // corner. `GetMirroredXWithWidthInView` is called to cancel out the mirroring
  // effect and returning the correct position for the bubble.
  gfx::Point anchor_position = GetAnchorView()->bounds().origin();
  gfx::Point origin;
  ConvertPointToTarget(GetAnchorView()->parent() /*source*/,
                       GetAnchorView()->GetWidget()->GetRootView() /*target*/,
                       &origin);
  origin.set_x(parent()->GetMirroredXWithWidthInView(
      origin.x(), GetAnchorView()->parent()->width()));
  anchor_position += origin.OffsetFromOrigin();
  auto bounds = GetBoundsAvailableToShowBubble();
  gfx::Size bubble_size(width() + 2 * horizontal_padding_,
                        height() + vertical_padding_);

  gfx::Point result;
  View* source;
  switch (positioning_strategy_) {
    case PositioningStrategy::kTryBeforeThenAfter:
      result = login_views_utils::CalculateBubblePositionBeforeAfterStrategy(
          {anchor_position, GetAnchorView()->size()}, bubble_size, bounds);
      source = GetAnchorView()->GetWidget()->GetRootView();
      break;
    case PositioningStrategy::kTryAfterThenBefore:
      result = login_views_utils::CalculateBubblePositionAfterBeforeStrategy(
          {anchor_position, GetAnchorView()->size()}, bubble_size, bounds);
      source = GetAnchorView()->GetWidget()->GetRootView();
      break;
    case PositioningStrategy::kShowAbove: {
      gfx::Point top_center = GetAnchorView()->bounds().top_center();
      result = top_center - gfx::Vector2d(GetPreferredSize().width() / 2,
                                          GetPreferredSize().height());
      source = GetAnchorView()->parent();
      break;
    }
    case PositioningStrategy::kShowBelow: {
      result = GetAnchorView()->bounds().bottom_left();
      source = GetAnchorView()->parent();
      break;
    }
  }
  // Get position of the bubble surrounded by paddings.
  result.Offset(horizontal_padding_, 0);
  ConvertPointToTarget(source /*source*/, parent() /*target*/, &result);
  return result;
}

void LoginBaseBubbleView::SetAnchorView(
    base::WeakPtr<views::View> anchor_view) {
  if (layer()) {
    layer()->GetAnimator()->StopAnimating();
  }
  anchor_view_ = std::move(anchor_view);
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

gfx::Size LoginBaseBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size;
  size.set_width(kBubbleTotalWidthDp);
  size.set_height(GetLayoutManager()->GetPreferredHeightForWidth(
      this, kBubbleTotalWidthDp));
  return size;
}

void LoginBaseBubbleView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // If layout occurs while the bubble is visible (i.e. due to Show()), its
  // bounds may change because of the parent's LayoutManager. This allows the
  // bubbles to always determine their own size and position.
  if (GetVisible()) {
    SetSize(GetPreferredSize());
    SetPosition(CalculatePosition());
  }
}

void LoginBaseBubbleView::OnBlur() {
  Hide();
}

void LoginBaseBubbleView::SetPadding(int horizontal_padding,
                                     int vertical_padding) {
  horizontal_padding_ = horizontal_padding;
  vertical_padding_ = vertical_padding;
}

gfx::Rect LoginBaseBubbleView::GetBoundsAvailableToShowBubble() const {
  auto bounds = GetRootViewBounds();
  auto work_area = GetWorkArea();
  // Get min means here to exclude either shelf or virtual keyaboard.
  bounds.set_height(std::min(bounds.height(), work_area.height()));
  return bounds;
}

views::View* LoginBaseBubbleView::GetAnchorView() const {
  if (anchor_view_.WasInvalidated()) {
    // TODO(crbug.com/1171827): This is to detect dangling anchor_view_
    // pointers.
    DUMP_WILL_BE_NOTREACHED();
  }
  return anchor_view_.get();
}

gfx::Rect LoginBaseBubbleView::GetRootViewBounds() const {
  if (!GetAnchorView()) {
    return gfx::Rect();
  }

  return GetAnchorView()->GetWidget()->GetRootView()->GetLocalBounds();
}

gfx::Rect LoginBaseBubbleView::GetWorkArea() const {
  if (!GetAnchorView()) {
    return gfx::Rect();
  }

  return screen_util::GetDisplayWorkAreaBoundsInParentForLockScreen(
      GetAnchorView()->GetWidget()->GetNativeWindow());
}

void LoginBaseBubbleView::ScheduleAnimation(bool visible) {
  if (GetBubbleOpener()) {
    views::InkDrop::Get(GetBubbleOpener())
        ->AnimateToState(visible ? views::InkDropState::ACTIVATED
                                 : views::InkDropState::DEACTIVATED,
                         nullptr /*event*/);
  }

  if (layer()) {
    layer()->GetAnimator()->StopAnimating();
  }

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

BEGIN_METADATA(LoginBaseBubbleView)
END_METADATA

}  // namespace ash
