// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_view.h"

#include <algorithm>
#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "base/containers/contains.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/strings/grit/ui_strings.h"
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

// Value should match the one in
// ash/resources/vector_icons/overview_window_close.icon.
constexpr int kCloseButtonIconMarginDp = 5;

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
    views::Button::PressedCallback close_callback,
    aura::Window* window,
    bool show_preview)
    : WindowMiniView(window),
      overview_item_(overview_item),
      close_button_(header_view()->icon_label_view()->AddChildView(
          std::make_unique<CloseButton>(
              std::move(close_callback),
              chromeos::features::IsJellyrollEnabled()
                  ? CloseButton::Type::kMediumFloating
                  : CloseButton::Type::kLargeFloating))) {
  DCHECK(overview_item_);
  // This should not be focusable. It's also to avoid accessibility error when
  // |window->GetTitle()| is empty.
  SetFocusBehavior(FocusBehavior::NEVER);

  views::InkDrop::Get(close_button_)
      ->SetMode(views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  close_button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  // Call this last as it calls |Layout()| which relies on the some of the other
  // elements existing.
  SetShowPreview(show_preview);
  // Do not show header if the current overview item is the drop target widget.
  if (overview_item_->overview_grid()->IsDropTargetWindow(
          overview_item_->GetWindow())) {
    header_view()->SetVisible(false);
    current_header_visibility_ = HeaderVisibility::kInvisible;
  }

  header_view()->UpdateIconView(window);
}

OverviewItemView::~OverviewItemView() = default;

void OverviewItemView::SetHeaderVisibility(HeaderVisibility visibility,
                                           bool animate) {
  DCHECK(header_view()->layer());
  if (visibility == current_header_visibility_) {
    return;
  }
  const HeaderVisibility previous_visibility = current_header_visibility_;
  current_header_visibility_ = visibility;

  const bool all_invisible = visibility == HeaderVisibility::kInvisible;
  if (animate) {
    AnimateLayerOpacity(header_view()->layer(), !all_invisible);
  } else {
    header_view()->layer()->SetOpacity(all_invisible ? 0.f : 1.f);
  }

  // If there is not a `close_button_`, then we are done.
  if (!close_button_) {
    return;
  }

  // If the whole header is fading out and there is a `close_button_`, then
  // we need to disable the close button without also fading the close button.
  if (all_invisible) {
    close_button_->SetEnabled(false);
    return;
  }

  const bool close_button_visible = visibility == HeaderVisibility::kVisible;
  // If `header_view()` was hidden and is fading in, set the opacity and enabled
  // state of `close_button_` depending on whether the close button should fade
  // in with `header_view()` or stay hidden. Or show the close button
  // immediately if we are not animating.
  if (previous_visibility == HeaderVisibility::kInvisible || !animate) {
    close_button_->layer()->SetOpacity(close_button_visible ? 1.f : 0.f);
    close_button_->SetEnabled(close_button_visible);
    return;
  }

  AnimateLayerOpacity(close_button_->layer(), close_button_visible);
  close_button_->SetEnabled(close_button_visible);
}

void OverviewItemView::HideCloseInstantlyAndThenShowItSlowly() {
  DCHECK(close_button_);
  DCHECK_NE(HeaderVisibility::kInvisible, current_header_visibility_);
  ui::Layer* layer = close_button_->layer();
  DCHECK(layer);

  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 0.f)
      .At(kCloseButtonSlowFadeInDelay)
      .SetDuration(kCloseButtonSlowFadeInDuration)
      .SetOpacity(layer, 1.f, gfx::Tween::FAST_OUT_SLOW_IN);

  current_header_visibility_ = HeaderVisibility::kVisible;
  close_button_->SetEnabled(true);
}

void OverviewItemView::OnOverviewItemWindowRestoring() {
  overview_item_ = nullptr;
  close_button_->ResetListener();
}

void OverviewItemView::RefreshPreviewView() {
  if (!preview_view())
    return;

  preview_view()->RecreatePreviews();
  Layout();
}

gfx::Rect OverviewItemView::GetHeaderBounds() const {
  if (chromeos::features::IsJellyrollEnabled()) {
    return WindowMiniView::GetHeaderBounds();
  }

  // We want to align the edges of the image as shown below in the diagram. The
  // resource itself contains some padding, which is the distance from the edges
  // of the image to the edges of the vector icon. The icon keeps its size in
  // dips and is centered in the middle of the preferred width, so the
  // additional padding would be equal to half the difference in width between
  // the preferred width and the image size. The resulting padding would be that
  // number plus the padding in the resource, in dips.
  const int image_width = kIconSize.width();
  const int close_button_width = close_button_->GetPreferredSize().width();
  const int right_padding =
      (close_button_width - image_width) / 2 + kCloseButtonIconMarginDp;

  // Positions the header in a way so that the right aligned close button is
  // aligned so that the edge of its icon, not the button lines up with the
  // margins. In the diagram below, a represents the the right edge of the
  // provided icon (which contains some padding), b represents the right edge of
  // |close_button_| and c represents the right edge of the local bounds.
  // ---------------------------+---------+
  // |                          |  +---+  |
  // |    |title_label_|        |  |   a  b
  // |                          |  +---+  |
  // ---------------------------+---------+
  //                                   c
  //                                   |

  // The size of this view is larger than that of the visible elements. Position
  // the header so that the margin is accounted for, then shift the right bounds
  // by a bit so that the close button resource lines up with the right edge of
  // the visible region.
  const gfx::Rect contents_bounds(GetContentsBounds());
  return gfx::Rect(contents_bounds.x(), contents_bounds.y(),
                   contents_bounds.width() + right_padding, kHeaderHeightDp);
}

gfx::Size OverviewItemView::GetPreviewViewSize() const {
  // The preview should expand to fit the bounds allocated for the content,
  // except if it is letterboxed or pillarboxed.
  const gfx::SizeF preview_pref_size(preview_view()->GetPreferredSize());
  const float aspect_ratio =
      preview_pref_size.width() / preview_pref_size.height();
  gfx::SizeF target_size(GetContentAreaBounds().size());
  OverviewGridWindowFillMode fill_mode =
      overview_item_ ? overview_item_->GetWindowDimensionsType()
                     : OverviewGridWindowFillMode::kNormal;
  switch (fill_mode) {
    case OverviewGridWindowFillMode::kNormal:
      break;
    case OverviewGridWindowFillMode::kLetterBoxed:
      target_size.set_height(target_size.width() / aspect_ratio);
      break;
    case OverviewGridWindowFillMode::kPillarBoxed:
      target_size.set_width(target_size.height() * aspect_ratio);
      break;
  }

  return gfx::ToRoundedSize(target_size);
}

views::View* OverviewItemView::GetView() {
  return this;
}

void OverviewItemView::MaybeActivateHighlightedView() {
  if (overview_item_)
    overview_item_->OnHighlightedViewActivated();
}

void OverviewItemView::MaybeCloseHighlightedView(bool primary_action) {
  if (overview_item_ && primary_action)
    overview_item_->OnHighlightedViewClosed();
}

void OverviewItemView::MaybeSwapHighlightedView(bool right) {}

bool OverviewItemView::MaybeActivateHighlightedViewOnOverviewExit(
    OverviewSession* overview_session) {
  DCHECK(overview_session);
  overview_session->SelectWindow(overview_item_);
  return true;
}

void OverviewItemView::OnViewHighlighted() {
  UpdateFocusState(/*focus=*/true);
}

void OverviewItemView::OnViewUnhighlighted() {
  UpdateFocusState(/*focus=*/false);
}

gfx::Point OverviewItemView::GetMagnifierFocusPointInScreen() {
  // When this item is tabbed into, put the magnifier focus on the front of the
  // title, so that users can read the title first thing.
  const gfx::Rect title_bounds =
      header_view()->title_label()->GetBoundsInScreen();
  return gfx::Point(GetMirroredXInView(title_bounds.x()),
                    title_bounds.CenterPoint().y());
}

bool OverviewItemView::OnMousePressed(const ui::MouseEvent& event) {
  if (!overview_item_)
    return views::View::OnMousePressed(event);
  overview_item_->HandleMouseEvent(event);
  return true;
}

bool OverviewItemView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!overview_item_)
    return views::View::OnMouseDragged(event);
  overview_item_->HandleMouseEvent(event);
  return true;
}

bool OverviewItemView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (!overview_item_) {
    return false;
  }
  overview_item_->overview_grid()->HandleMouseWheelScrollEvent(
      event.y_offset());
  return true;
}

void OverviewItemView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!overview_item_) {
    views::View::OnMouseReleased(event);
    return;
  }
  overview_item_->HandleMouseEvent(event);
}

void OverviewItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (!overview_item_)
    return;

  overview_item_->HandleGestureEvent(event);
  event->SetHandled();
}

bool OverviewItemView::CanAcceptEvent(const ui::Event& event) {
  bool accept_events = true;
  // Do not process or accept press down events that are on the border.
  static ui::EventType press_types[] = {ui::ET_GESTURE_TAP_DOWN,
                                        ui::ET_MOUSE_PRESSED};
  if (event.IsLocatedEvent() && base::Contains(press_types, event.type())) {
    const gfx::Rect content_bounds = GetContentsBounds();
    if (!content_bounds.Contains(event.AsLocatedEvent()->location()))
      accept_events = false;
  }

  return accept_events && views::View::CanAcceptEvent(event);
}

void OverviewItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  WindowMiniView::GetAccessibleNodeData(node_data);

  // TODO: This doesn't allow |this| to be navigated by ChromeVox, find a way
  // to allow |this| as well as the title and close button.
  node_data->role = ax::mojom::Role::kGenericContainer;
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kDescription,
      l10n_util::GetStringUTF8(
          IDS_ASH_OVERVIEW_CLOSABLE_HIGHLIGHT_ITEM_A11Y_EXTRA_TIP));
}

void OverviewItemView::OnThemeChanged() {
  WindowMiniView::OnThemeChanged();
  UpdateFocusState(IsViewHighlighted());
}

BEGIN_METADATA(OverviewItemView, WindowMiniView)
END_METADATA

}  // namespace ash
