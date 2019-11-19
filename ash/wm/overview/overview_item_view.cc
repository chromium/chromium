// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/window_preview_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Duration of the show/hide animation of the header.
constexpr base::TimeDelta kHeaderFadeDuration =
    base::TimeDelta::FromMilliseconds(167);

// Delay before the show animation of the header.
constexpr base::TimeDelta kHeaderFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);

// Duration of the slow show animation of the close button.
constexpr base::TimeDelta kCloseButtonSlowFadeInDuration =
    base::TimeDelta::FromMilliseconds(300);

// Delay before the slow show animation of the close button.
constexpr base::TimeDelta kCloseButtonSlowFadeInDelay =
    base::TimeDelta::FromMilliseconds(750);

constexpr int kCloseButtonInkDropInsetDp = 2;

constexpr SkColor kCloseButtonColor = SK_ColorWHITE;

// Value should match the one in
// ash/resources/vector_icons/overview_window_close.icon.
constexpr int kCloseButtonIconMarginDp = 5;

// The colors of the close button ripple.
constexpr SkColor kCloseButtonInkDropRippleColor =
    SkColorSetA(kCloseButtonColor, 0x0F);
constexpr SkColor kCloseButtonInkDropRippleHighlightColor =
    SkColorSetA(kCloseButtonColor, 0x14);

// Animates |layer| from 0 -> 1 opacity if |visible| and 1 -> 0 opacity
// otherwise. The tween type differs for |visible| and if |visible| is true
// there is a slight delay before the animation begins. Does not animate if
// opacity matches |visible|.
void AnimateLayerOpacity(ui::Layer* layer, bool visible) {
  float target_opacity = visible ? 1.f : 0.f;
  if (layer->GetTargetOpacity() == target_opacity)
    return;

  layer->SetOpacity(1.f - target_opacity);
  gfx::Tween::Type tween =
      visible ? gfx::Tween::LINEAR_OUT_SLOW_IN : gfx::Tween::FAST_OUT_LINEAR_IN;
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kHeaderFadeDuration);
  settings.SetTweenType(tween);
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  if (visible) {
    layer->GetAnimator()->SchedulePauseForProperties(
        kHeaderFadeInDelay, ui::LayerAnimationElement::OPACITY);
  }
  layer->SetOpacity(target_opacity);
}

// The close button for the overview item. It has a custom ink drop.
class OverviewCloseButton : public views::ImageButton {
 public:
  explicit OverviewCloseButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
    SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kOverviewWindowCloseIcon, kCloseButtonColor));
    SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    SetMinimumImageSize(gfx::Size(kHeaderHeightDp, kHeaderHeightDp));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
    SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  }

  ~OverviewCloseButton() override = default;

  // Resets the listener so that the listener can go out of scope.
  void ResetListener() { listener_ = nullptr; }

 protected:
  // views::Button:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    auto ink_drop = std::make_unique<views::InkDropImpl>(this, size());
    ink_drop->SetAutoHighlightMode(
        views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
    ink_drop->SetShowHighlightOnHover(true);
    return ink_drop;
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
        kCloseButtonInkDropRippleColor, /*visible_opacity=*/1.f);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        std::make_unique<views::CircleLayerDelegate>(
            kCloseButtonInkDropRippleHighlightColor, GetInkDropRadius()));
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), GetInkDropRadius());
  }

 private:
  int GetInkDropRadius() const {
    return std::min(size().width(), size().height()) / 2 -
           kCloseButtonInkDropInsetDp;
  }

  DISALLOW_COPY_AND_ASSIGN(OverviewCloseButton);
};

}  // namespace

OverviewItemView::OverviewItemView(OverviewItem* overview_item,
                                   aura::Window* window,
                                   bool show_preview)
    : WindowMiniView(window, /*views_should_paint_to_layers=*/true),
      overview_item_(overview_item) {
  DCHECK(overview_item_);
  // This should not be focusable. It's also to avoid accessibility error when
  // |window->GetTitle()| is empty.
  SetFocusBehavior(FocusBehavior::NEVER);

  close_button_ = new OverviewCloseButton(overview_item_);
  close_button_->SetPaintToLayer();
  close_button_->layer()->SetFillsBoundsOpaquely(false);
  AddChildViewOf(header_view(), close_button_);

  // Call this last as it calls |Layout()| which relies on the some of the other
  // elements existing.
  SetShowPreview(show_preview);
  // Do not show header if the current overview item is the drop target widget.
  if (show_preview || overview_item_->overview_grid()->IsDropTargetWindow(
                          overview_item_->GetWindow())) {
    header_view()->layer()->SetOpacity(0.f);
    current_header_visibility_ = HeaderVisibility::kInvisible;
  }
}

OverviewItemView::~OverviewItemView() = default;

void OverviewItemView::SetHeaderVisibility(HeaderVisibility visibility) {
  DCHECK(header_view()->layer());
  if (visibility == current_header_visibility_)
    return;
  const HeaderVisibility previous_visibility = current_header_visibility_;
  current_header_visibility_ = visibility;

  const bool all_invisible = visibility == HeaderVisibility::kInvisible;
  AnimateLayerOpacity(header_view()->layer(), !all_invisible);

  // If |header_view()| is fading out, we are done. Depending on if the close
  // button was visible, it will fade out with |header_view()| or stay hidden.
  if (all_invisible || !close_button_)
    return;

  const bool close_button_visible = visibility == HeaderVisibility::kVisible;
  // If |header_view()| was hidden and is fading in, set the opacity of
  // |close_button_| depending on whether the close button should fade in with
  // |header_view()| or stay hidden.
  if (previous_visibility == HeaderVisibility::kInvisible) {
    close_button_->layer()->SetOpacity(close_button_visible ? 1.f : 0.f);
    return;
  }

  AnimateLayerOpacity(close_button_->layer(), close_button_visible);
}

void OverviewItemView::HideCloseInstantlyAndThenShowItSlowly() {
  DCHECK(close_button_);
  DCHECK_NE(HeaderVisibility::kInvisible, current_header_visibility_);
  ui::Layer* layer = close_button_->layer();
  DCHECK(layer);
  current_header_visibility_ = HeaderVisibility::kVisible;
  layer->SetOpacity(0.f);
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kCloseButtonSlowFadeInDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  layer->GetAnimator()->SchedulePauseForProperties(
      kCloseButtonSlowFadeInDelay, ui::LayerAnimationElement::OPACITY);
  layer->SetOpacity(1.f);
}

void OverviewItemView::OnOverviewItemWindowRestoring() {
  overview_item_ = nullptr;
  static_cast<OverviewCloseButton*>(close_button_)->ResetListener();
}

void OverviewItemView::RefreshPreviewView() {
  if (!preview_view())
    return;

  preview_view()->RecreatePreviews();
  Layout();
}

void OverviewItemView::UpdatePreviewRoundedCorners(bool show, float rounding) {
  if (!preview_view())
    return;

  DCHECK(preview_view()->layer());
  const float scale = preview_view()->layer()->transform().Scale2d().x();
  const gfx::RoundedCornersF radii(show ? rounding / scale : 0.0f);
  preview_view()->layer()->SetRoundedCornerRadius(radii);
  preview_view()->layer()->SetIsFastRoundedCorner(true);
}

int OverviewItemView::GetMargin() const {
  return kOverviewMargin;
}

gfx::Rect OverviewItemView::GetHeaderBounds() const {
  // The size of the close button resource, |image_width| is scaled up to
  // |close_button_width| when laid out. The resource itself contains some
  // padding, which is the distance from the edges of the image to the edges of
  // the vector icon. We want to align the edges of the image as shown below in
  // the diagram. The resource padding, like the rest of the resource is scaled
  // up, so calculate the scaled up resource padding.
  const int image_width =
      close_button_->GetImage(views::ImageButton::STATE_NORMAL).width();
  const int close_button_width = close_button_->width();
  const int right_padding = gfx::ToRoundedInt(
      kCloseButtonIconMarginDp * float{close_button_width} / image_width);
  const int margin = GetMargin();

  // Positions the header in a way so that the right aligned close button is
  // aligned so that the edge of its icon, not the button lines up with the
  // margins. In the diagram below, a represents the the right edge of the
  // provided icon (which contains some padding), b represents the right edge of
  // |close_button_| and c represents the right edge of the local bounds.
  // ---------------------------+---------+
  //                            |  +---+  |
  //      |title_label_|        |  |   a  b
  //                            |  +---+  |
  // ---------------------------+---------+
  //                                   c
  //                                   |
  return gfx::Rect(margin, margin, GetLocalBounds().width() - right_padding,
                   kHeaderHeightDp);
}

views::View* OverviewItemView::GetView() {
  return this;
}

gfx::Rect OverviewItemView::GetHighlightBoundsInScreen() {
  // Use the target bounds instead of |GetBoundsInScreen()| because |this| may
  // be animating. However, the origin will be incorrect because the windows are
  // always positioned above and left of the parents origin, then translated. To
  // get the proper origin we use |GetBoundsInScreen()| which takes into account
  // the transform (but returns the wrong height and width).
  auto* window = GetWidget()->GetNativeWindow();
  gfx::Rect target_bounds = window->GetTargetBounds();
  target_bounds.set_origin(window->GetBoundsInScreen().origin());
  target_bounds.Inset(kWindowMargin, kWindowMargin);
  return target_bounds;
}

void OverviewItemView::MaybeActivateHighlightedView() {
  if (overview_item_)
    overview_item_->OnHighlightedViewActivated();
}

void OverviewItemView::MaybeCloseHighlightedView() {
  if (overview_item_)
    overview_item_->OnHighlightedViewClosed();
}

gfx::Point OverviewItemView::GetMagnifierFocusPointInScreen() {
  // When this item is tabbed into, put the magnifier focus on the front of the
  // title, so that users can read the title first thing.
  const gfx::Rect title_bounds = title_label()->GetBoundsInScreen();
  return gfx::Point(GetMirroredXInView(title_bounds.x()),
                    title_bounds.CenterPoint().y());
}

const char* OverviewItemView::GetClassName() const {
  return "OverviewItemView";
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

  if (overview_item_->ShouldIgnoreGestureEvents()) {
    event->SetHandled();
    return;
  }

  overview_item_->HandleGestureEvent(event);
  event->SetHandled();
}

bool OverviewItemView::CanAcceptEvent(const ui::Event& event) {
  bool accept_events = true;
  // Do not process or accept press down events that are on the border.
  static ui::EventType press_types[] = {ui::ET_GESTURE_TAP_DOWN,
                                        ui::ET_MOUSE_PRESSED};
  if (event.IsLocatedEvent() && base::Contains(press_types, event.type())) {
    gfx::Rect inset_bounds = GetLocalBounds();
    inset_bounds.Inset(gfx::Insets(kOverviewMargin));
    if (!inset_bounds.Contains(event.AsLocatedEvent()->location()))
      accept_events = false;
  }

  return accept_events && views::View::CanAcceptEvent(event);
}

void OverviewItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  WindowMiniView::GetAccessibleNodeData(node_data);
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kDescription,
      l10n_util::GetStringUTF8(
          IDS_ASH_OVERVIEW_CLOSABLE_HIGHLIGHT_ITEM_A11Y_EXTRA_TIP));
}

}  // namespace ash
