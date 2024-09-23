// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using assistant::util::CreateLayerAnimationSequence;
using assistant::util::CreateOpacityElement;
using assistant::util::StartLayerAnimationSequence;

// Animation.
constexpr base::TimeDelta kChipFadeInDuration = base::Milliseconds(250);
constexpr base::TimeDelta kChipFadeOutDuration = base::Milliseconds(200);

// Metrics.
constexpr char kAssistantSuggestionChipHistogram[] =
    "Ash.Assistant.AnimationSmoothness.SuggestionChip";

constexpr int kPreferredHeightDip = 64;

}  // namespace

// SuggestionChipAnimator -----------------------------------------------------

class SuggestionChipAnimator : public ElementAnimator {
 public:
  SuggestionChipAnimator(SuggestionChipView* chip,
                         const SuggestionContainerView* parent)
      : ElementAnimator(chip), parent_(parent) {}

  SuggestionChipAnimator(const SuggestionChipAnimator&) = delete;
  SuggestionChipAnimator& operator=(const SuggestionChipAnimator&) = delete;

  ~SuggestionChipAnimator() override = default;

  void AnimateIn(ui::CallbackLayerAnimationObserver* observer) override {
    StartLayerAnimationSequence(
        layer()->GetAnimator(), CreateAnimateInAnimation(), observer,
        base::BindRepeating<void(const std::string&, int)>(
            base::UmaHistogramPercentage, kAssistantSuggestionChipHistogram));
  }

  void AnimateOut(ui::CallbackLayerAnimationObserver* observer) override {
    StartLayerAnimationSequence(
        layer()->GetAnimator(), CreateAnimateOutAnimation(), observer,
        base::BindRepeating<void(const std::string&, int)>(
            base::UmaHistogramPercentage, kAssistantSuggestionChipHistogram));
  }

  void FadeOut(ui::CallbackLayerAnimationObserver* observer) override {
    // If the user pressed a chip we do not fade it out.
    if (!IsSelectedChip())
      ElementAnimator::FadeOut(observer);
  }

 private:
  bool IsSelectedChip() const { return view() == parent_->selected_chip(); }

  ui::LayerAnimationSequence* CreateAnimateInAnimation() const {
    return CreateLayerAnimationSequence(CreateOpacityElement(
        1.f, kChipFadeInDuration, gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  ui::LayerAnimationSequence* CreateAnimateOutAnimation() const {
    return CreateLayerAnimationSequence(CreateOpacityElement(
        0.f, kChipFadeOutDuration, gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  const raw_ptr<const SuggestionContainerView>
      parent_;  // |parent_| owns |this|.
};

// SuggestionContainerView -----------------------------------------------------

SuggestionContainerView::SuggestionContainerView(
    AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate) {
  SetID(AssistantViewID::kSuggestionContainer);
  InitLayout();

  AssistantSuggestionsController::Get()->GetModel()->AddObserver(this);
  AssistantUiController::Get()->GetModel()->AddObserver(this);
}

SuggestionContainerView::~SuggestionContainerView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);

  if (AssistantSuggestionsController::Get())
    AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
}

gfx::Size SuggestionContainerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(INT_MAX, kPreferredHeightDip);
}

void SuggestionContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  // Our contents should never be smaller than our container width because when
  // showing conversation starters we will be center aligned.
  const int width =
      std::max(content_view->GetPreferredSize().width(), this->width());
  content_view->SetSize(gfx::Size(width, kPreferredHeightDip));
}

void SuggestionContainerView::OnAssistantControllerDestroying() {
  AnimatedContainerView::OnAssistantControllerDestroying();

  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
}

void SuggestionContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  AnimatedContainerView::OnCommittedQueryChanged(query);

  // Cache the fact that a query has been committed in this Assistant session so
  // that we know to stop handling conversation starter updates.
  has_committed_query_ = true;
}

void SuggestionContainerView::InitLayout() {
  layout_manager_ =
      content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(0, assistant::ui::kHorizontalPadding),
          /*between_child_spacing=*/kSpacingDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // We center align when showing conversation starters.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

void SuggestionContainerView::OnConversationStartersChanged(
    const std::vector<AssistantSuggestion>& conversation_starters) {
  // We don't show conversation starters when showing onboarding since the
  // onboarding experience already provides the user w/ suggestions.
  if (delegate()->ShouldShowOnboarding()) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHLauncherSearchHelpUiFeature)) {
    return;
  }

  // If we've committed a query we should ignore changes to the cache of
  // conversation starters as we are past the state in which they should be
  // presented. To present them now could incorrectly associate the conversation
  // starters with a response.
  if (has_committed_query_) {
    return;
  }

  RemoveAllViews();
  OnSuggestionsAdded(conversation_starters);
}

std::unique_ptr<ElementAnimator> SuggestionContainerView::HandleSuggestion(
    const AssistantSuggestion& suggestion) {
  // When no longer showing conversation starters, we start align our content.
  layout_manager_->set_main_axis_alignment(
      has_committed_query_ ? views::BoxLayout::MainAxisAlignment::kStart
                           : views::BoxLayout::MainAxisAlignment::kCenter);

  return AddSuggestionChip(suggestion);
}

void SuggestionContainerView::OnAllViewsRemoved() {
  // Clear the selected button.
  selected_chip_ = nullptr;
}

std::unique_ptr<ElementAnimator> SuggestionContainerView::AddSuggestionChip(
    const AssistantSuggestion& suggestion) {
  auto suggestion_chip_view =
      std::make_unique<SuggestionChipView>(delegate(), suggestion);
  suggestion_chip_view->SetCallback(base::BindRepeating(
      &SuggestionContainerView::OnButtonPressed, base::Unretained(this),
      base::Unretained(suggestion_chip_view.get())));

  // The chip will be animated on its own layer.
  suggestion_chip_view->SetPaintToLayer();
  suggestion_chip_view->layer()->SetFillsBoundsOpaquely(false);
  suggestion_chip_view->layer()->SetOpacity(0.f);

  // Add to the view hierarchy and return the animator for the suggestion chip.
  return std::make_unique<SuggestionChipAnimator>(
      contents()->AddChildView(std::move(suggestion_chip_view)), this);
}

void SuggestionContainerView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  if (assistant::util::IsStartingSession(new_visibility, old_visibility) &&
      entry_point.value() != AssistantEntryPoint::kLauncherSearchResult) {
    // Show conversation starters at the start of a new Assistant session except
    // when the user already started a query in Launcher quick search box (QSB).
    OnConversationStartersChanged(AssistantSuggestionsController::Get()
                                      ->GetModel()
                                      ->GetConversationStarters());
    return;
  }

  if (!assistant::util::IsFinishingSession(new_visibility))
    return;

  // When Assistant is finishing a session, we need to reset state.
  has_committed_query_ = false;

  // When we start a new session we will be showing conversation starters so
  // we need to center align our content.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

void SuggestionContainerView::InitializeUIForBubbleView() {
  OnConversationStartersChanged(AssistantSuggestionsController::Get()
                                    ->GetModel()
                                    ->GetConversationStarters());
}

void SuggestionContainerView::OnButtonPressed(SuggestionChipView* chip_view) {
  // Remember which chip was selected, so we can give it a special animation.
  selected_chip_ = chip_view;
  delegate()->OnSuggestionPressed(selected_chip_->suggestion_id());
}

BEGIN_METADATA(SuggestionContainerView)
END_METADATA

}  // namespace ash
