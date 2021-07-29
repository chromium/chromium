// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include <string>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view_factory.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "cc/base/math_util.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPaddingBottomDip = 8;
constexpr int kScrollIndicatorHeightDip = 1;

}  // namespace

// UiElementContainerView ------------------------------------------------------

UiElementContainerView::UiElementContainerView(AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate),
      view_factory_(std::make_unique<AssistantUiElementViewFactory>(delegate)) {
  SetID(AssistantViewID::kUiElementContainer);
  InitLayout();
}

UiElementContainerView::~UiElementContainerView() = default;

const char* UiElementContainerView::GetClassName() const {
  return "UiElementContainerView";
}

gfx::Size UiElementContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int UiElementContainerView::GetHeightForWidth(int width) const {
  return content_view()->GetHeightForWidth(width);
}

gfx::Size UiElementContainerView::GetMinimumSize() const {
  // AssistantMainStage uses BoxLayout's flex property to grow/shrink
  // UiElementContainerView to fill available space as needed. When height is
  // shrunk to zero, as is temporarily the case during the initial container
  // growth animation for the first Assistant response, UiElementContainerView
  // will be laid out with zero width. We do not recover from this state until
  // the next layout pass, which causes Assistant cards for the first response
  // to be laid out with zero width. We work around this by imposing a minimum
  // height restriction of 1 dip that is factored into BoxLayout's flex
  // calculations to make sure that our width is never being set to zero.
  return gfx::Size(INT_MAX, 1);
}

void UiElementContainerView::Layout() {
  AnimatedContainerView::Layout();

  // Scroll indicator.
  scroll_indicator_->SetBounds(0, height() - kScrollIndicatorHeightDip, width(),
                               kScrollIndicatorHeightDip);
}

void UiElementContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  const int preferred_height = content_view->GetHeightForWidth(width());
  content_view->SetSize(gfx::Size(width(), preferred_height));
}

void UiElementContainerView::InitLayout() {
  // Content.
  content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kHorizontalMarginDip, kPaddingBottomDip,
                  kHorizontalMarginDip),
      kSpacingDip));

  // Scroll indicator.
  scroll_indicator_ = AddChildView(std::make_unique<views::View>());
  scroll_indicator_->SetBackground(
      views::CreateSolidBackground(gfx::kGoogleGrey300));

  // The scroll indicator paints to its own layer which is animated in/out using
  // implicit animation settings.
  scroll_indicator_->SetPaintToLayer();
  scroll_indicator_->layer()->SetAnimator(
      ui::LayerAnimator::CreateImplicitAnimator());
  scroll_indicator_->layer()->SetFillsBoundsOpaquely(false);
  scroll_indicator_->layer()->SetOpacity(0.f);

  // We cannot draw |scroll_indicator_| over Assistant cards due to issues w/
  // layer ordering. Because |kScrollIndicatorHeightDip| is sufficiently small,
  // we'll use an empty bottom border to reserve space for |scroll_indicator_|.
  // When |scroll_indicator_| is not visible, this just adds a negligible amount
  // of margin to the bottom of the content. Otherwise, |scroll_indicator_| will
  // occupy this space.
  SetBorder(views::CreateEmptyBorder(0, 0, kScrollIndicatorHeightDip, 0));
}

void UiElementContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  // Scroll to the top to play nice with the transition animation.
  ScrollToPosition(vertical_scroll_bar(), 0);
  AnimatedContainerView::OnCommittedQueryChanged(query);
}

std::unique_ptr<ElementAnimator> UiElementContainerView::HandleUiElement(
    const AssistantUiElement* ui_element) {
  // Create a new view for the |ui_element|.
  auto view = view_factory_->Create(ui_element);

  // If the first UI element is a card, it has a unique margin requirement.
  const bool is_card = ui_element->type() == AssistantUiElementType::kCard;
  const bool is_first_ui_element = content_view()->children().empty();
  if (is_card && is_first_ui_element) {
    constexpr int kMarginTopDip = 24;
    view->SetBorder(views::CreateEmptyBorder(kMarginTopDip, 0, 0, 0));
  }

  // Add the view to the hierarchy and prepare its animation layer for entry.
  auto* view_ptr = content_view()->AddChildView(std::move(view));
  view_ptr->GetLayerForAnimating()->SetOpacity(0.f);

  // Return the animator that will be used to animate the view.
  return view_ptr->CreateAnimator();
}

void UiElementContainerView::OnAllViewsAnimatedIn() {
  const auto* response =
      AssistantInteractionController::Get()->GetModel()->response();
  DCHECK(response);

  // Let screen reader read the query result. This includes the text response
  // and the card fallback text, but webview result is not included. We don't
  // read when there is TTS to avoid speaking over the server response.
  if (!response->has_tts())
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void UiElementContainerView::OnScrollBarUpdated(views::ScrollBar* scroll_bar,
                                                int viewport_size,
                                                int content_size,
                                                int content_scroll_offset) {
  if (scroll_bar != vertical_scroll_bar())
    return;

  // When the vertical scroll bar is updated, we update our |scroll_indicator_|.
  bool can_scroll = content_size > (content_scroll_offset + viewport_size);
  UpdateScrollIndicator(can_scroll);
}

void UiElementContainerView::OnScrollBarVisibilityChanged(
    views::ScrollBar* scroll_bar,
    bool is_visible) {
  // When the vertical scroll bar is hidden, we need to update our
  // |scroll_indicator_|. This may occur during a layout pass when the new
  // content no longer requires a vertical scroll bar while the old content did.
  if (scroll_bar == vertical_scroll_bar() && !is_visible)
    UpdateScrollIndicator(/*can_scroll=*/false);
}

void UiElementContainerView::UpdateScrollIndicator(bool can_scroll) {
  const float target_opacity = can_scroll ? 1.f : 0.f;

  ui::Layer* layer = scroll_indicator_->layer();
  if (!cc::MathUtil::IsWithinEpsilon(layer->GetTargetOpacity(), target_opacity))
    layer->SetOpacity(target_opacity);
}

}  // namespace ash
