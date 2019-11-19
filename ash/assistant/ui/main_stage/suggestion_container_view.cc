// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using assistant::util::CreateLayerAnimationSequence;
using assistant::util::CreateOpacityElement;
using assistant::util::CreateTransformElement;
using assistant::util::StartLayerAnimationSequence;
using assistant::util::StartLayerAnimationSequencesTogether;

// Animation.
constexpr int kChipMoveUpDistanceDip = 24;
constexpr base::TimeDelta kSelectedChipAnimateInDelay =
    base::TimeDelta::FromMilliseconds(150);
constexpr base::TimeDelta kChipFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kChipMoveUpDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kChipFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);

// Appearance.
constexpr int kPreferredHeightDip = 48;

}  // namespace

// SuggestionChipAnimator -----------------------------------------------------

class SuggestionChipAnimator : public ElementAnimator {
 public:
  SuggestionChipAnimator(SuggestionChipView* chip,
                         const SuggestionContainerView* parent)
      : ElementAnimator(chip), parent_(parent) {}
  ~SuggestionChipAnimator() override = default;

  void AnimateIn(ui::CallbackLayerAnimationObserver* observer) override {
    // As part of the animation we will move up the chip from the bottom
    // so we need to start by moving it down.
    MoveDown();
    layer()->SetOpacity(0.f);

    StartLayerAnimationSequencesTogether(layer()->GetAnimator(),
                                         {
                                             CreateFadeInAnimation(),
                                             CreateMoveUpAnimation(),
                                         },
                                         observer);
  }

  void AnimateOut(ui::CallbackLayerAnimationObserver* observer) override {
    StartLayerAnimationSequence(layer()->GetAnimator(),
                                CreateAnimateOutAnimation(), observer);
  }

  void FadeOut(ui::CallbackLayerAnimationObserver* observer) override {
    // If the user pressed a chip we do not fade it out.
    if (!IsSelectedChip())
      ElementAnimator::FadeOut(observer);
  }

 private:
  bool IsSelectedChip() const { return view() == parent_->selected_chip(); }

  void MoveDown() const {
    gfx::Transform transform;
    transform.Translate(0, kChipMoveUpDistanceDip);
    layer()->SetTransform(transform);
  }

  ui::LayerAnimationSequence* CreateFadeInAnimation() const {
    return CreateLayerAnimationSequence(
        ui::LayerAnimationElement::CreatePauseElement(
            ui::LayerAnimationElement::AnimatableProperty::OPACITY,
            kSelectedChipAnimateInDelay),
        CreateOpacityElement(1.f, kChipFadeInDuration,
                             gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  ui::LayerAnimationSequence* CreateMoveUpAnimation() const {
    return CreateLayerAnimationSequence(
        ui::LayerAnimationElement::CreatePauseElement(
            ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
            kSelectedChipAnimateInDelay),
        CreateTransformElement(gfx::Transform(), kChipMoveUpDuration,
                               gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  ui::LayerAnimationSequence* CreateAnimateOutAnimation() const {
    return CreateLayerAnimationSequence(CreateOpacityElement(
        0.f, kChipFadeOutDuration, gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  const SuggestionContainerView* const parent_;  // |parent_| owns |this|.

  DISALLOW_COPY_AND_ASSIGN(SuggestionChipAnimator);
};

// SuggestionContainerView -----------------------------------------------------

SuggestionContainerView::SuggestionContainerView(
    AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate) {
  SetID(AssistantViewID::kSuggestionContainer);
  InitLayout();

  // The AssistantViewDelegate should outlive SuggestionContainerView.
  delegate->AddSuggestionsModelObserver(this);
  delegate->AddUiModelObserver(this);
}

SuggestionContainerView::~SuggestionContainerView() {
  delegate()->RemoveUiModelObserver(this);
  delegate()->RemoveSuggestionsModelObserver(this);
  delegate()->RemoveInteractionModelObserver(this);
}

const char* SuggestionContainerView::GetClassName() const {
  return "SuggestionContainerView";
}

gfx::Size SuggestionContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int SuggestionContainerView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void SuggestionContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  // Our contents should never be smaller than our container width because when
  // showing conversation starters we will be center aligned.
  const int width =
      std::max(content_view->GetPreferredSize().width(), this->width());
  content_view->SetSize(gfx::Size(width, kPreferredHeightDip));
}

void SuggestionContainerView::InitLayout() {
  layout_manager_ =
      content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip), kSpacingDip));

  layout_manager_->set_cross_axis_alignment(
      app_list_features::IsAssistantLauncherUIEnabled()
          ? views::BoxLayout::CrossAxisAlignment::kCenter
          : views::BoxLayout::CrossAxisAlignment::kEnd);

  // We center align when showing conversation starters.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

void SuggestionContainerView::OnConversationStartersChanged(
    const std::map<int, const AssistantSuggestion*>& conversation_starters) {
  RemoveAllViews();
  OnSuggestionsChanged(conversation_starters);
  AnimateIn();
}

void SuggestionContainerView::HandleResponse(
    const AssistantResponse& response) {
  has_received_response_ = true;

  // When no longer showing conversation starters, we start align our content.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  OnSuggestionsChanged(response.GetSuggestions());
}

void SuggestionContainerView::OnAllViewsRemoved() {
  // Abort any download requests in progress.
  download_request_weak_factory_.InvalidateWeakPtrs();

  // Clear our view cache.
  suggestion_chip_views_.clear();

  // Clear the selected button.
  selected_chip_ = nullptr;

  // Note that we don't reset |has_received_response_| here because that refers
  // to whether we've received a response during the current Assistant session,
  // not whether we are currently displaying a response.
}

void SuggestionContainerView::OnSuggestionsChanged(
    const std::map<int, const AssistantSuggestion*>& suggestions) {
  for (const auto& suggestion : suggestions) {
    // We will use the same identifier by which the Assistant interaction model
    // uniquely identifies a suggestion to uniquely identify its corresponding
    // suggestion chip view.
    AddSuggestionChip(/*suggestion=*/*suggestion.second,
                      /*id=*/suggestion.first);
  }
}

void SuggestionContainerView::AddSuggestionChip(
    const AssistantSuggestion& suggestion,
    int id) {
  SuggestionChipView::Params params;
  params.text = base::UTF8ToUTF16(suggestion.text);

  if (!suggestion.icon_url.is_empty()) {
    // Initiate a request to download the image for the suggestion chip icon.
    // Note that the request is identified by the suggestion id.
    delegate()->DownloadImage(
        suggestion.icon_url,
        base::BindOnce(&SuggestionContainerView::OnSuggestionChipIconDownloaded,
                       download_request_weak_factory_.GetWeakPtr(), id));

    // To reserve layout space until the actual icon can be downloaded, we
    // supply an empty placeholder image to the suggestion chip view.
    params.icon = gfx::ImageSkia();
  }

  SuggestionChipView* suggestion_chip_view =
      new SuggestionChipView(params, /*listener=*/this);

  suggestion_chip_view->SetAccessibleName(params.text);

  // Given a suggestion chip view, we need to be able to look up the id of
  // the underlying suggestion. This is used for handling press events.
  suggestion_chip_view->SetID(id);

  // The chip will be animated on its own layer.
  suggestion_chip_view->SetPaintToLayer();
  suggestion_chip_view->layer()->SetFillsBoundsOpaquely(false);

  // Given an id, we also want to be able to look up the corresponding
  // suggestion chip view. This is used for handling icon download events.
  suggestion_chip_views_[id] = suggestion_chip_view;

  content_view()->AddChildView(suggestion_chip_view);

  // Set the animations for the suggestion chip.
  AddElementAnimator(
      std::make_unique<SuggestionChipAnimator>(suggestion_chip_view, this));
}

void SuggestionContainerView::OnSuggestionChipIconDownloaded(
    int id,
    const gfx::ImageSkia& icon) {
  if (!icon.isNull())
    suggestion_chip_views_[id]->SetIcon(icon);
}

void SuggestionContainerView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  // Remember which chip was selected, so we can give it a special animation.
  selected_chip_ = suggestion_chip_views_[sender->GetID()];

  const AssistantSuggestion* suggestion = nullptr;

  // If we haven't yet received a query response, the suggestion chip that was
  // pressed was a conversation starter.
  if (!has_received_response_) {
    suggestion = delegate()->GetSuggestionsModel()->GetConversationStarterById(
        sender->GetID());
  } else {
    // Otherwise, the suggestion chip belonged to the interaction response.
    suggestion =
        delegate()->GetInteractionModel()->response()->GetSuggestionById(
            sender->GetID());
  }

  delegate()->OnSuggestionChipPressed(suggestion);
}

void SuggestionContainerView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (assistant::util::IsStartingSession(new_visibility, old_visibility) &&
      entry_point.value() != AssistantEntryPoint::kLauncherSearchResult) {
    // Show conversation starters at the start of a new Assistant session except
    // when the user already started a query in Launcher quick search box (QSB).
    OnConversationStartersChanged(
        delegate()->GetSuggestionsModel()->GetConversationStarters());
    return;
  }

  if (!assistant::util::IsFinishingSession(new_visibility))
    return;

  // When Assistant is finishing a session, we need to reset view state.
  has_received_response_ = false;

  // When we start a new session we will be showing conversation starters so
  // we need to center align our content.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

}  // namespace ash
