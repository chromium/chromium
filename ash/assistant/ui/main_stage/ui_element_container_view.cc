// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include <string>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "ash/assistant/ui/main_stage/assistant_card_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using assistant::util::CreateLayerAnimationSequence;
using assistant::util::CreateOpacityElement;
using assistant::util::CreateTransformElement;
using assistant::util::StartLayerAnimationSequence;

// Appearance.
constexpr int kEmbeddedUiFirstCardMarginTopDip = 8;
constexpr int kEmbeddedUiPaddingBottomDip = 8;
constexpr int kMainUiFirstCardMarginTopDip = 40;
constexpr int kMainUiPaddingBottomDip = 24;

// Main UI element animation.
constexpr base::TimeDelta kMainUiElementAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kMainUiElementAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kMainUiElementAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);
// Text elements must fade out to 0 as the thinking dots will appear in the
// location of the first text element.
constexpr float kMainUiTextElementAnimationFadeOutOpacity = 0.f;

// Embedded UI element animation.
constexpr base::TimeDelta kEmbeddedUiElementAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kEmbeddedUiElementAnimationMoveUpDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kEmbeddedUiElementAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);
constexpr int kEmbeddedUiElementAnimationMoveUpDistanceDip = 32;

// Helpers ---------------------------------------------------------------------

int GetFirstCardMarginTopDip() {
  return app_list_features::IsAssistantLauncherUIEnabled()
             ? kEmbeddedUiFirstCardMarginTopDip
             : kMainUiFirstCardMarginTopDip;
}

int GetPaddingBottomDip() {
  return app_list_features::IsAssistantLauncherUIEnabled()
             ? kEmbeddedUiPaddingBottomDip
             : kMainUiPaddingBottomDip;
}

// Animator for elements in the main (non-embedded) UI.
class MainUiAnimator : public ElementAnimator {
 public:
  using ElementAnimator::ElementAnimator;
  ~MainUiAnimator() override = default;

  // ElementAnimator:
  void AnimateOut(ui::CallbackLayerAnimationObserver* observer) override {
    StartLayerAnimationSequence(
        layer()->GetAnimator(),
        CreateLayerAnimationSequence(CreateOpacityElement(
            kMinimumAnimateOutOpacity, kMainUiElementAnimationFadeOutDuration,
            gfx::Tween::Type::FAST_OUT_SLOW_IN)),
        observer);
  }

  void AnimateIn(ui::CallbackLayerAnimationObserver* observer) override {
    // We fade in the views to full opacity after a slight delay.
    assistant::util::StartLayerAnimationSequence(
        layer()->GetAnimator(),
        CreateLayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                kMainUiElementAnimationFadeInDelay),
            CreateOpacityElement(1.f, kMainUiElementAnimationFadeInDuration)),
        observer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MainUiAnimator);
};

// Animator used for card elements in the main (non-embedded) UI.
class MainUiCardAnimator : public MainUiAnimator {
 public:
  // Constructor used for card elements.
  explicit MainUiCardAnimator(AssistantCardElementView* element)
      : MainUiAnimator(element), element_(element) {}

  ui::Layer* layer() const override { return element_->native_view()->layer(); }

 private:
  AssistantCardElementView* const element_;

  DISALLOW_COPY_AND_ASSIGN(MainUiCardAnimator);
};

// Animator used for text elements in the main (non-embedded) UI.
class MainUiTextAnimator : public MainUiAnimator {
 public:
  // Constructor used for text elements.
  explicit MainUiTextAnimator(AssistantTextElementView* element)
      : MainUiAnimator(element) {}

  void FadeOut(ui::CallbackLayerAnimationObserver* observer) override {
    assistant::util::StartLayerAnimationSequence(
        layer()->GetAnimator(),
        assistant::util::CreateLayerAnimationSequence(
            assistant::util::CreateOpacityElement(
                kMainUiTextElementAnimationFadeOutOpacity, kFadeOutDuration)),
        observer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MainUiTextAnimator);
};

// Animator for elements in the embedded UI.
class EmbeddedUiAnimator : public ElementAnimator {
 public:
  using ElementAnimator::ElementAnimator;
  ~EmbeddedUiAnimator() override = default;

  // ElementAnimator:
  void AnimateOut(ui::CallbackLayerAnimationObserver* observer) override {
    StartLayerAnimationSequence(
        layer()->GetAnimator(),
        CreateLayerAnimationSequence(
            CreateOpacityElement(kMinimumAnimateOutOpacity,
                                 kEmbeddedUiElementAnimationFadeOutDuration)),
        observer);
  }

  void AnimateIn(ui::CallbackLayerAnimationObserver* observer) override {
    // As part of the animation we will move up the element from the bottom
    // so we need to start by moving it down.
    MoveElementDown();

    assistant::util::StartLayerAnimationSequencesTogether(
        layer()->GetAnimator(),
        {
            CreateFadeInAnimation(),
            CreateMoveUpAnimation(),
        },
        observer);
  }

 private:
  void MoveElementDown() const {
    gfx::Transform transform;
    transform.Translate(0, kEmbeddedUiElementAnimationMoveUpDistanceDip);
    layer()->SetTransform(transform);
  }

  ui::LayerAnimationSequence* CreateFadeInAnimation() const {
    return CreateLayerAnimationSequence(
        CreateOpacityElement(1.f, kEmbeddedUiElementAnimationFadeInDuration,
                             gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  ui::LayerAnimationSequence* CreateMoveUpAnimation() const {
    return CreateLayerAnimationSequence(CreateTransformElement(
        gfx::Transform(), kEmbeddedUiElementAnimationMoveUpDuration,
        gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  DISALLOW_COPY_AND_ASSIGN(EmbeddedUiAnimator);
};

// Animator for card elements in the embedded UI.
class EmbeddedUiCardAnimator : public EmbeddedUiAnimator {
 public:
  // Constructor used for card elements.
  explicit EmbeddedUiCardAnimator(AssistantCardElementView* element)
      : EmbeddedUiAnimator(element), element_(element) {}

  ui::Layer* layer() const override { return element_->native_view()->layer(); }

 private:
  AssistantCardElementView* const element_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedUiCardAnimator);
};

// Animator for text elements in the embedded UI.
using EmbeddedUiTextAnimator = EmbeddedUiAnimator;

std::unique_ptr<ElementAnimator> CreateCardAnimator(
    AssistantCardElementView* card_element) {
  if (app_list_features::IsAssistantLauncherUIEnabled())
    return std::make_unique<EmbeddedUiCardAnimator>(card_element);
  else
    return std::make_unique<MainUiCardAnimator>(card_element);
}

std::unique_ptr<ElementAnimator> CreateTextAnimator(
    AssistantTextElementView* text_element) {
  if (app_list_features::IsAssistantLauncherUIEnabled())
    return std::make_unique<EmbeddedUiTextAnimator>(text_element);
  else
    return std::make_unique<MainUiTextAnimator>(text_element);
}

}  // namespace

// UiElementContainerView ------------------------------------------------------

UiElementContainerView::UiElementContainerView(AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate) {
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

void UiElementContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  const int preferred_height = content_view->GetHeightForWidth(width());
  content_view->SetSize(gfx::Size(width(), preferred_height));
}

void UiElementContainerView::InitLayout() {
  content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kUiElementHorizontalMarginDip, GetPaddingBottomDip(),
                  kUiElementHorizontalMarginDip),
      kSpacingDip));
}

void UiElementContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  // Scroll to the top to play nice with the transition animation.
  ScrollToPosition(vertical_scroll_bar(), 0);

  AnimatedContainerView::OnCommittedQueryChanged(query);
}

void UiElementContainerView::HandleResponse(const AssistantResponse& response) {
  for (const auto& ui_element : response.GetUiElements()) {
    switch (ui_element->GetType()) {
      case AssistantUiElementType::kCard:
        OnCardElementAdded(
            static_cast<const AssistantCardElement*>(ui_element.get()));
        break;
      case AssistantUiElementType::kText:
        OnTextElementAdded(
            static_cast<const AssistantTextElement*>(ui_element.get()));
        break;
    }
  }
}

void UiElementContainerView::OnCardElementAdded(
    const AssistantCardElement* card_element) {
  // The card, for some reason, is not embeddable so we'll have to ignore it.
  if (!card_element->contents())
    return;

  auto* card_element_view =
      new AssistantCardElementView(delegate(), card_element);
  if (is_first_card_) {
    is_first_card_ = false;

    // The first card requires a top margin of |GetFirstCardMarginTopDip()|, but
    // we need to account for child spacing because the first card is not
    // necessarily the first UI element.
    const int top_margin_dip =
        GetFirstCardMarginTopDip() - (children().empty() ? 0 : kSpacingDip);

    // We effectively create a top margin by applying an empty border.
    card_element_view->SetBorder(
        views::CreateEmptyBorder(top_margin_dip, 0, 0, 0));
  }

  content_view()->AddChildView(card_element_view);

  // The view will be animated on its own layer, so we need to do some initial
  // layer setup. We're going to fade the view in, so hide it.
  card_element_view->native_view()->layer()->SetFillsBoundsOpaquely(false);
  card_element_view->native_view()->layer()->SetOpacity(0.f);

  // We set the animator to handle all animations for this view.
  AddElementAnimator(CreateCardAnimator(card_element_view));
}

void UiElementContainerView::OnTextElementAdded(
    const AssistantTextElement* text_element) {
  auto* text_element_view = new AssistantTextElementView(text_element);

  // The view will be animated on its own layer, so we need to do some initial
  // layer setup. We're going to fade the view in, so hide it.
  text_element_view->SetPaintToLayer();
  text_element_view->layer()->SetFillsBoundsOpaquely(false);
  text_element_view->layer()->SetOpacity(0.f);

  content_view()->AddChildView(text_element_view);

  // We set the animator to handle all animations for this view.
  AddElementAnimator(CreateTextAnimator(text_element_view));
}

void UiElementContainerView::OnAllViewsRemoved() {
  // Reset state for the next response.
  is_first_card_ = true;
}

void UiElementContainerView::OnAllViewsAnimatedIn() {
  // Let screen reader read the query result. This includes the text response
  // and the card fallback text, but webview result is not included.
  // We don't read when there is TTS to avoid speaking over the server response.
  const AssistantResponse* response =
      delegate()->GetInteractionModel()->response();
  if (!response->has_tts())
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

}  // namespace ash
