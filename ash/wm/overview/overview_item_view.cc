// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_view.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "base/containers/contains.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Duration of the show/hide animation of the header.
constexpr base::TimeDelta kHeaderFadeDuration = base::Milliseconds(167);

// Delay before the show animation of the header.
constexpr base::TimeDelta kHeaderFadeInDelay = base::Milliseconds(83);

// Duration of the slow show animation of the close button.
constexpr base::TimeDelta kCloseButtonSlowFadeInDuration =
    base::Milliseconds(300);

// Delay before the slow show animation of the close button.
constexpr base::TimeDelta kCloseButtonSlowFadeInDelay = base::Milliseconds(750);

// Animates |layer| from 0 -> 1 opacity if |visible| and 1 -> 0 opacity
// otherwise. The tween type differs for |visible| and if |visible| is true
// there is a slight delay before the animation begins. Does not animate if
// opacity matches |visible|.
void AnimateLayerOpacity(ui::Layer* layer, bool visible) {
  float target_opacity = visible ? 1.f : 0.f;
  if (layer->GetTargetOpacity() == target_opacity)
    return;

  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 1.f - target_opacity)
      .At(visible ? kHeaderFadeInDelay : base::TimeDelta())
      .SetDuration(kHeaderFadeDuration)
      .SetOpacity(layer, target_opacity,
                  visible ? gfx::Tween::LINEAR_OUT_SLOW_IN
                          : gfx::Tween::FAST_OUT_LINEAR_IN);
}

}  // namespace

OverviewItemView::OverviewItemView(
    OverviewItem* overview_item,
    EventHandlerDelegate* event_handler_delegate,
    views::Button::PressedCallback close_callback,
    aura::Window* window,
    bool show_preview)
    : WindowMiniView(window, /*use_custom_focus_predicate=*/false),
      overview_item_(overview_item),
      event_handler_delegate_(event_handler_delegate),
      close_button_(header_view()->icon_label_view()->AddChildView(
          std::make_unique<CloseButton>(std::move(close_callback),
                                        CloseButton::Type::kMediumFloating))) {
  CHECK(overview_item_);
  // Focusable so we can add accelerators to this view.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  views::InkDrop::Get(close_button_)
      ->SetMode(views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  close_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  close_button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  header_view()->UpdateIconView(window);

  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, 0));
  AddAccelerator(ui::Accelerator(ui::VKEY_SPACE, 0));

  // Call this last as it triggers layout, which relies on some of the other
  // elements existing.
  SetShowPreview(show_preview);

  // TODO: This doesn't allow |this| to be navigated by ChromeVox, find a way
  // to allow |this| as well as the title and close button.
  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  UpdateAccessibleDescription();
}

OverviewItemView::~OverviewItemView() = default;

void OverviewItemView::SetCloseButtonVisible(bool visible) {
  if (!close_button_->layer()) {
    close_button_->SetPaintToLayer();
    close_button_->layer()->SetFillsBoundsOpaquely(false);
  }

  AnimateLayerOpacity(close_button_->layer(), visible);
  close_button_->SetEnabled(visible);
}

void OverviewItemView::HideCloseInstantlyAndThenShowItSlowly() {
  CHECK(close_button_);

  if (!close_button_->layer()) {
    close_button_->SetPaintToLayer();
    close_button_->layer()->SetFillsBoundsOpaquely(false);
  }

  ui::Layer* layer = close_button_->layer();

  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 0.f)
      .At(kCloseButtonSlowFadeInDelay)
      .SetDuration(kCloseButtonSlowFadeInDuration)
      .SetOpacity(layer, 1.f, gfx::Tween::FAST_OUT_SLOW_IN);

  close_button_->SetEnabled(true);
}

void OverviewItemView::OnOverviewItemWindowRestoring() {
  // Explicitly reset `overview_item_` and `event_handler_delegate_` to avoid
  // dangling pointer since the corresponding `item_widget_` may outlive its
  // corresponding `overview_item_` see `FadeOutWidgetFromOverview()` in
  // `overview_utils.cc` for example.
  overview_item_ = nullptr;
  event_handler_delegate_ = nullptr;
  close_button_->ResetListener();
}

void OverviewItemView::RefreshPreviewView() {
  if (!preview_view())
    return;

  preview_view()->RecreatePreviews();
  DeprecatedLayoutImmediately();
}

void OverviewItemView::AcceptSelection(OverviewSession* overview_session) {
  CHECK(overview_session);
  overview_session->SelectWindow(overview_item_);
}

gfx::Size OverviewItemView::GetPreviewViewSize() const {
  // The preview should expand to fit the bounds allocated for the content,
  // except if it is letterboxed or pillarboxed.
  const gfx::SizeF preview_pref_size(preview_view()->GetPreferredSize());
  const float aspect_ratio =
      preview_pref_size.width() / preview_pref_size.height();
  gfx::SizeF target_size(GetContentAreaBounds().size());
  OverviewItemFillMode fill_mode =
      overview_item_ ? overview_item_->GetOverviewItemFillMode()
                     : OverviewItemFillMode::kNormal;
  switch (fill_mode) {
    case OverviewItemFillMode::kNormal:
      break;
    case OverviewItemFillMode::kLetterBoxed:
      target_size.set_height(target_size.width() / aspect_ratio);
      break;
    case OverviewItemFillMode::kPillarBoxed:
      target_size.set_width(target_size.height() * aspect_ratio);
      break;
  }

  return gfx::ToRoundedSize(target_size);
}

void OverviewItemView::RefreshItemVisuals() {
  // `overview_item_` may get reset in `OnOverviewItemWindowRestoring()` since
  // the corresponding `item_widget_` may outlive its corresponding
  // `overview_item_`, we want to avoid `overview_item_` from being accessed
  // again.
  if (!overview_item_) {
    return;
  }

  // Set the rounded corners to accommodate for the customized rounded corners
  // needed for the overview group item.
  if (SnapGroupController* snap_group_controller = SnapGroupController::Get()) {
    const aura::Window* window = overview_item_->GetWindow();
    if (snap_group_controller->GetSnapGroupForGivenWindow(window)) {
      SetRoundedCornersRadius(window_util::GetMiniWindowRoundedCorners(
          window, /*include_header_rounding=*/true));

      // `SetRoundedCornersRadius()` will trigger rounded corners update for
      // header view, preview view and focus ring automatically. Early return to
      // avoid duplicate updates.
      return;
    }
  }

  ResetRoundedCorners();
}

bool OverviewItemView::OnMousePressed(const ui::MouseEvent& event) {
  if (!event_handler_delegate_) {
    return views::View::OnMousePressed(event);
  }

  event_handler_delegate_->HandleMouseEvent(event, overview_item_);
  return true;
}

bool OverviewItemView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!event_handler_delegate_) {
    return views::View::OnMouseDragged(event);
  }

  event_handler_delegate_->HandleMouseEvent(event, overview_item_);
  return true;
}

void OverviewItemView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event_handler_delegate_) {
    views::View::OnMouseReleased(event);
    return;
  }

  event_handler_delegate_->HandleMouseEvent(event, overview_item_);
}

void OverviewItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (!event_handler_delegate_) {
    return;
  }

  event_handler_delegate_->HandleGestureEvent(event, overview_item_);
  event->SetHandled();
}

bool OverviewItemView::CanAcceptEvent(const ui::Event& event) {
  bool accept_events = true;
  // Do not process or accept press down events that are on the border.
  static ui::EventType press_types[] = {ui::EventType::kGestureTapDown,
                                        ui::EventType::kMousePressed};
  if (event.IsLocatedEvent() && base::Contains(press_types, event.type())) {
    const gfx::Rect content_bounds = GetContentsBounds();
    if (!content_bounds.Contains(event.AsLocatedEvent()->location()))
      accept_events = false;
  }

  return accept_events && views::View::CanAcceptEvent(event);
}

bool OverviewItemView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.IsCtrlDown() && accelerator.key_code() == ui::VKEY_W) {
    if (overview_item_) {
      overview_item_->OnFocusedViewClosed();
    }
    return true;
  }

  if (accelerator.key_code() == ui::VKEY_RETURN ||
      accelerator.key_code() == ui::VKEY_SPACE) {
    if (overview_item_) {
      overview_item_->OnFocusedViewActivated();
    }
    return true;
  }
  return WindowMiniView::AcceleratorPressed(accelerator);
}

bool OverviewItemView::CanHandleAccelerators() const {
  return HasFocus() && WindowMiniView::CanHandleAccelerators();
}

void OverviewItemView::OnWindowDestroying(aura::Window* window) {
  WindowMiniView::OnWindowDestroying(window);
  UpdateAccessibleDescription();
}

void OverviewItemView::UpdateAccessibleDescription() {
  const bool is_group_item = [&]() {
    auto* snap_group_controller = SnapGroupController::Get();
    return snap_group_controller &&
           snap_group_controller->GetSnapGroupForGivenWindow(source_window());
  }();

  GetViewAccessibility().SetDescription(l10n_util::GetStringUTF8(
      is_group_item ? IDS_ASH_SNAP_GROUP_WINDOW_CYCLE_DESCRIPTION
                    : IDS_ASH_OVERVIEW_CLOSABLE_HIGHLIGHT_ITEM_A11Y_EXTRA_TIP));
}

BEGIN_METADATA(OverviewItemView)
END_METADATA

}  // namespace ash
