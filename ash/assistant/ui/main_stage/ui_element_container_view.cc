// Copyright 2018 The Chromium Authors
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
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/base/math_util.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPaddingBottomDip = 8;
constexpr int kScrollIndicatorHeightDip = 1;

// ObservableOverflowIndicator allows a caller to observe visibility change of
// an overflow indicator. Note that we are using this view with setting
// thickness to 0 with ScrollView::SetCustomOverflowIndicator. This view is not
// visible.
class ObservableOverflowIndicator : public views::View {
  METADATA_HEADER(ObservableOverflowIndicator, views::View)

 public:
  explicit ObservableOverflowIndicator(
      UiElementContainerView* ui_element_container_view)
      : ui_element_container_view_(ui_element_container_view) {}

 protected:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override {
    if (starting_from != this)
      return;

    ui_element_container_view_->OnOverflowIndicatorVisibilityChanged(
        is_visible);
  }

 private:
  raw_ptr<UiElementContainerView> ui_element_container_view_ = nullptr;
};

BEGIN_METADATA(ObservableOverflowIndicator)
END_METADATA

// This is views::View. We define InvisibleOverflowIndicator as we can add
// METADATA to this view. We set thickness of this view to 0 with
// ScrollView::SetCustomOverflowIndicator. The background of this view is NOT
// transparent, i.e. it becomes visible if you set thickness larger than 0.
class InvisibleOverflowIndicator : public views::View {
  METADATA_HEADER(InvisibleOverflowIndicator, views::View)
};

BEGIN_METADATA(InvisibleOverflowIndicator)
END_METADATA

}  // namespace

// UiElementContainerView ------------------------------------------------------

UiElementContainerView::UiElementContainerView(AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate),
      view_factory_(std::make_unique<AssistantUiElementViewFactory>(delegate)) {
  SetID(AssistantViewID::kUiElementContainer);
  InitLayout();
}

UiElementContainerView::~UiElementContainerView() = default;

gfx::Size UiElementContainerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(INT_MAX, content_view()->GetHeightForWidth(INT_MAX));
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

void UiElementContainerView::Layout(PassKey) {
  LayoutSuperclass<AnimatedContainerView>(this);

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
  const int horizontal_margin = assistant::ui::kHorizontalMargin;
  content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(0, horizontal_margin, kPaddingBottomDip,
                        horizontal_margin),
      kSpacingDip));

  // Scroll indicator.
  scroll_indicator_ = AddChildView(std::make_unique<views::View>());
  scroll_indicator_->SetID(kOverflowIndicator);
  scroll_indicator_->SetBackground(
      views::CreateSolidBackground(GetOverflowIndicatorBackgroundColor()));

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
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, 0, kScrollIndicatorHeightDip, 0)));

  // We set invisible overflow indicators with thickness=0. But we observe
  // visibility change of the bottom indicator.
  SetDrawOverflowIndicator(true);
  SetCustomOverflowIndicator(
      views::OverflowIndicatorAlignment::kBottom,
      std::make_unique<ObservableOverflowIndicator>(this),
      /*thickness=*/0, /*fills_opaquely=*/true);
  SetCustomOverflowIndicator(views::OverflowIndicatorAlignment::kTop,
                             std::make_unique<InvisibleOverflowIndicator>(),
                             /*thickness=*/0,
                             /*fills_opaquely=*/true);
  SetCustomOverflowIndicator(views::OverflowIndicatorAlignment::kLeft,
                             std::make_unique<InvisibleOverflowIndicator>(),
                             /*thickness=*/0,
                             /*fills_opaquely=*/true);
  SetCustomOverflowIndicator(views::OverflowIndicatorAlignment::kRight,
                             std::make_unique<InvisibleOverflowIndicator>(),
                             /*thickness=*/0,
                             /*fills_opaquely=*/true);
}

void UiElementContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  // Scroll to the top to play nice with the transition animation.
  ScrollToPosition(vertical_scroll_bar(), 0);
  AnimatedContainerView::OnCommittedQueryChanged(query);
}

void UiElementContainerView::OnThemeChanged() {
  views::View::OnThemeChanged();

  scroll_indicator_->background()->SetNativeControlColor(
      GetOverflowIndicatorBackgroundColor());

  // SetNativeControlColor doesn't trigger a repaint.
  scroll_indicator_->SchedulePaint();
}

std::unique_ptr<ElementAnimator> UiElementContainerView::HandleUiElement(
    const AssistantUiElement* ui_element) {
  // Create a new view for the |ui_element|.
  auto view = view_factory_->Create(ui_element);
  if (!view) {
    return nullptr;
  }

  // If the first UI element is a card, it has a unique margin requirement.
  const bool is_card = ui_element->type() == AssistantUiElementType::kCard;
  const bool is_first_ui_element = content_view()->children().empty();
  if (is_card && is_first_ui_element) {
    constexpr int kMarginTopDip = 24;
    view->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(kMarginTopDip, 0, 0, 0)));
  }

  // Add the view to the hierarchy and prepare its animation layer for entry.
  auto* view_ptr = content_view()->AddChildView(std::move(view));

  // If this runs in test, AssistantCardElement can use TestAshWebView. It does
  // not return a native view. We cannot obtain a layer for animation. We want
  // to add it to the UI tree as a test is going to interact with it. But we
  // skip an animation.
  if (is_card && !view_ptr->GetLayerForAnimating())
    return nullptr;

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

void UiElementContainerView::OnOverflowIndicatorVisibilityChanged(
    bool is_visible) {
  const float target_opacity = is_visible ? 1.f : 0.f;

  ui::Layer* layer = scroll_indicator_->layer();
  if (!cc::MathUtil::IsWithinEpsilon(layer->GetTargetOpacity(), target_opacity))
    layer->SetOpacity(target_opacity);
}

SkColor UiElementContainerView::GetOverflowIndicatorBackgroundColor() const {
  return ColorProvider::Get()->GetContentLayerColor(
      ColorProvider::ContentLayerType::kSeparatorColor);
}

BEGIN_METADATA(UiElementContainerView)
END_METADATA

}  // namespace ash
