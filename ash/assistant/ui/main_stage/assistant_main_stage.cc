// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_main_stage.h"

#include <algorithm>
#include <map>
#include <numeric>

#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/base/stack_layout.h"
#include "ash/assistant/ui/main_stage/assistant_footer_view.h"
#include "ash/assistant/ui/main_stage/assistant_header_view.h"
#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {

namespace {

using assistant::util::CreateLayerAnimationSequence;
using assistant::util::CreateOpacityElement;
using assistant::util::CreateTransformElement;
using assistant::util::StartLayerAnimationSequence;
using assistant::util::StartLayerAnimationSequencesTogether;

// Appearance.
constexpr int kGreetingLabelMarginTopDip = 28;
constexpr int kProgressIndicatorMarginLeftDip = 32;
constexpr int kProgressIndicatorMarginTopDip = 40;

// Animation.
// TODO(dmblack): Rename these constants to refer to the specific views they are
// animating to avoid confusion.
constexpr base::TimeDelta kAnimationExitFadeDuration =
    base::TimeDelta::FromMilliseconds(50);
constexpr int kAnimationExitTranslationDip = -103;
constexpr base::TimeDelta kAnimationExitTranslateDuration =
    base::TimeDelta::FromMilliseconds(333);
constexpr base::TimeDelta kAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(200);
constexpr base::TimeDelta kAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationTranslateUpDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationTranslateUpDuration =
    base::TimeDelta::FromMilliseconds(333);

// Footer animation.
constexpr int kFooterAnimationTranslationDip = 22;
constexpr base::TimeDelta kFooterAnimationTranslationDelay =
    base::TimeDelta::FromMilliseconds(66);
constexpr base::TimeDelta kFooterAnimationTranslationDuration =
    base::TimeDelta::FromMilliseconds(416);
constexpr base::TimeDelta kFooterAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(149);
constexpr base::TimeDelta kFooterAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kFooterAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(100);

// Footer entry animation.
constexpr base::TimeDelta kFooterEntryAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(283);
constexpr base::TimeDelta kFooterEntryAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);

// Greeting animation.
constexpr base::TimeDelta kGreetingAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(33);
constexpr base::TimeDelta kGreetingAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kGreetingAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kGreetingAnimationTranslateUpDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr int kGreetingAnimationTranslationDip = 115;

// Pending query animation.
constexpr base::TimeDelta kPendingQueryAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(433);

// Progress animation.
constexpr base::TimeDelta kProgressAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(233);
constexpr base::TimeDelta kProgressAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kProgressAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);

}  // namespace

// AssistantMainStage ----------------------------------------------------------

AssistantMainStage::AssistantMainStage(AssistantViewDelegate* delegate)
    : delegate_(delegate),
      active_query_exit_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_ended_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnActiveQueryExitAnimationEnded,
                  base::Unretained(this)))),
      footer_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_started_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnFooterAnimationStarted,
                  base::Unretained(this)),
              /*animation_ended_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnFooterAnimationEnded,
                  base::Unretained(this)))) {
  InitLayout();

  // The view hierarchy will be destructed before Shell, which owns
  // AssistantViewDelegate, so AssistantViewDelegate is guaranteed to outlive
  // the AssistantMainStage.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

AssistantMainStage::~AssistantMainStage() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* AssistantMainStage::GetClassName() const {
  return "AssistantMainStage";
}

void AssistantMainStage::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewBoundsChanged(views::View* view) {
  if (view == active_query_view_) {
    UpdateTopPadding();
  } else if (view == committed_query_view_) {
    UpdateQueryViewTransform(committed_query_view_);
  } else if (view == pending_query_view_) {
    UpdateQueryViewTransform(pending_query_view_);
  } else if (view == query_layout_container_) {
    if (committed_query_view_)
      UpdateQueryViewTransform(committed_query_view_);
    if (pending_query_view_)
      UpdateQueryViewTransform(pending_query_view_);
  }
}

void AssistantMainStage::OnViewPreferredSizeChanged(views::View* view) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewVisibilityChanged(views::View* view,
                                                 views::View* starting_view) {
  PreferredSizeChanged();
}

void AssistantMainStage::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // The children of AssistantMainStage will be animated on their own layers and
  // we want them to be clipped by their parent layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  InitContentLayoutContainer();
  InitQueryLayoutContainer();
  InitOverlayLayoutContainer();
}

void AssistantMainStage::InitContentLayoutContainer() {
  // Note that we will observe children of |content_layout_container_| to handle
  // preferred size and visibility change events in AssistantMainStage. This is
  // necessary because |content_layout_container_| may not change size in
  // response to these events, necessitating an explicit layout pass.
  content_layout_container_ = new views::View();

  views::BoxLayout* layout_manager =
      content_layout_container_->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical));

  // Header.
  header_ = new AssistantHeaderView(delegate_);
  content_layout_container_->AddChildView(header_);

  // UI element container.
  ui_element_container_ = new UiElementContainerView(delegate_);
  ui_element_container_->AddObserver(this);
  content_layout_container_->AddChildView(ui_element_container_);

  layout_manager->SetFlexForView(ui_element_container_, 1,
                                 /*use_min_size=*/true);

  // Footer.
  // Note that the |footer_| is placed within its own view container so that as
  // its visibility changes, its parent container will still reserve the same
  // layout space. This prevents jank that would otherwise occur due to
  // |ui_element_container_| claiming that empty space.
  views::View* footer_container = new views::View();
  footer_container->SetLayoutManager(std::make_unique<views::FillLayout>());

  footer_ = new AssistantFooterView(delegate_);
  footer_->AddObserver(this);

  // The footer will be animated on its own layer.
  footer_->SetPaintToLayer();
  footer_->layer()->SetFillsBoundsOpaquely(false);

  footer_container->AddChildView(footer_);
  content_layout_container_->AddChildView(footer_container);

  AddChildView(content_layout_container_);
}

void AssistantMainStage::InitQueryLayoutContainer() {
  // Note that we will observe children of |query_layout_container_| to handle
  // preferred size and visibility change events in AssistantMainStage. This is
  // necessary because |query_layout_container_| may not change size in response
  // to these events, thereby requiring an explicit layout pass.
  query_layout_container_ = new views::View();
  query_layout_container_->AddObserver(this);
  query_layout_container_->set_can_process_events_within_subtree(false);
  query_layout_container_->SetLayoutManager(std::make_unique<StackLayout>());

  AddChildView(query_layout_container_);
}

void AssistantMainStage::InitOverlayLayoutContainer() {
  // The overlay layout container is a view which is laid out on top of both
  // the content and query layout containers. As such, its children appear over
  // top of and do not cause repositioning to any of content/query layout's
  // underlying views. Events pass through the overlay layout container.
  overlay_layout_container_ = new views::View();
  overlay_layout_container_->set_can_process_events_within_subtree(false);

  auto* stack_layout = overlay_layout_container_->SetLayoutManager(
      std::make_unique<StackLayout>());

  // Greeting label.
  greeting_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
  greeting_label_->SetAutoColorReadabilityEnabled(false);
  greeting_label_->SetBorder(
      views::CreateEmptyBorder(kGreetingLabelMarginTopDip, 0, 0, 0));
  greeting_label_->SetEnabledColor(kTextColorPrimary);
  greeting_label_->SetFontList(
      assistant::ui::GetDefaultFontList()
          .DeriveWithSizeDelta(8)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  greeting_label_->SetMultiLine(true);

  // The greeting label will be animated on its own layer.
  greeting_label_->SetPaintToLayer();
  greeting_label_->layer()->SetFillsBoundsOpaquely(false);

  overlay_layout_container_->AddChildView(greeting_label_);

  // We need to stretch |greeting_label_| to match its parent so that it
  // won't use heuristics in Label to infer line breaking, which seems to cause
  // text clipping with DPI adjustment. See b/112843496.
  stack_layout->SetRespectDimensionForView(
      greeting_label_, StackLayout::RespectDimension::kHeight);

  // Progress indicator.
  progress_indicator_ = new AssistantProgressIndicator();
  progress_indicator_->SetBorder(
      views::CreateEmptyBorder(kProgressIndicatorMarginTopDip, 0, 0, 0));

  // The progress indicator will be animated on its own layer.
  progress_indicator_->SetPaintToLayer();
  progress_indicator_->layer()->SetFillsBoundsOpaquely(false);
  progress_indicator_->layer()->SetOpacity(0.f);

  overlay_layout_container_->AddChildView(progress_indicator_);

  AddChildView(overlay_layout_container_);
}

void AssistantMainStage::OnCommittedQueryChanged(const AssistantQuery& query) {
  if (is_first_query_) {
    // Hide the greeting label (for the first query)...
    assistant::util::FadeOutAndHide(greeting_label_,
                                    kGreetingAnimationFadeOutDuration);
  }

  // ...and always show the progress indicator.
  progress_indicator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          // Delay...
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kProgressAnimationFadeInDelay),
          // ...then fade in.
          CreateOpacityElement(1.f, kProgressAnimationFadeInDuration)));

  // After the first query, the progress indicator should be left aligned.
  if (!is_first_query_) {
    const int overlay_width = overlay_layout_container_->width();
    const int indicator_width = progress_indicator_->width();
    const int translation = -(overlay_width - indicator_width) / 2 +
                            kProgressIndicatorMarginLeftDip;

    gfx::Transform transform;
    transform.Translate(translation, 0);

    progress_indicator_->layer()->SetTransform(transform);
  }

  is_first_query_ = false;

  // The pending query has been committed. Update our pointers.
  committed_query_view_ = pending_query_view_;
  pending_query_view_ = nullptr;

  // Update the view.
  committed_query_view_->SetQuery(query);
}

void AssistantMainStage::OnActivateQuery() {
  if (!committed_query_view_)
    return;

  // Clear the previously active query.
  OnActiveQueryCleared();

  active_query_view_ = committed_query_view_;
  committed_query_view_ = nullptr;

  // Upon response delivery, we consider a query active. The view for the query
  // should animate from the bottom of its parent to the top.
  active_query_view_->layer()->GetAnimator()->StartTogether(
      {// Animate transformation.
       CreateLayerAnimationSequence(
           // Pause...
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
               kAnimationTranslateUpDelay),
           // ...then translate up to top of parent.
           CreateTransformElement(gfx::Transform(),
                                  kAnimationTranslateUpDuration,
                                  gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
       // Animate opacity.
       CreateLayerAnimationSequence(
           // Fade out to 0%...
           CreateOpacityElement(0.f, kAnimationFadeOutDuration),
           // ...then pause...
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::OPACITY,
               kAnimationFadeInDelay),
           // ...then fade in to 100%.
           CreateOpacityElement(1.f, kAnimationFadeInDuration))});

  UpdateTopPadding();
  UpdateFooter();
}

void AssistantMainStage::OnActiveQueryCleared() {
  if (!active_query_view_)
    return;

  // The active query view will translate off stage.
  gfx::Transform transform;
  transform.Translate(0, kAnimationExitTranslationDip);

  // Animate the exit of the action query view.
  StartLayerAnimationSequencesTogether(
      active_query_view_->layer()->GetAnimator(),
      {// Animate transformation.
       CreateLayerAnimationSequence(
           CreateTransformElement(transform, kAnimationExitTranslateDuration,
                                  gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
       // Animate opacity to 0%.
       CreateLayerAnimationSequence(
           CreateOpacityElement(0.f, kAnimationExitFadeDuration))},
      // Observe the animation.
      active_query_exit_animation_observer_.get());

  // Set the animation observer to active so that we receive callback events.
  active_query_exit_animation_observer_->SetActive();

  // Note that we have not yet deleted the query view but it will be cleaned up
  // when the animation observer callback runs.
  active_query_view_ = nullptr;
}

bool AssistantMainStage::OnActiveQueryExitAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The exited active query view will always be the first child of its parent.
  delete query_layout_container_->children().front();

  // TODO(https://crbug.com/896079): Remove this when view.cc handles the
  // event notification.
  query_layout_container_->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged, false);

  UpdateTopPadding();

  // Return false to prevent the observer from destroying itself.
  return false;
}

void AssistantMainStage::AnimateInGreetingLabel() {
  // We're going to animate the greeting label up into position so we'll
  // need to apply an initial transformation.
  gfx::Transform transform;
  transform.Translate(0, kGreetingAnimationTranslationDip);

  // Set up our pre-animation values.
  greeting_label_->layer()->SetOpacity(0.f);
  greeting_label_->layer()->SetTransform(transform);
  greeting_label_->SetVisible(true);

  // Start animating greeting label.
  greeting_label_->layer()->GetAnimator()->StartTogether(
      {// Animate the transformation.
       CreateLayerAnimationSequence(CreateTransformElement(
           gfx::Transform(), kGreetingAnimationTranslateUpDuration,
           gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
       // Animate the opacity to 100% with delay.
       CreateLayerAnimationSequence(
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::OPACITY,
               kGreetingAnimationFadeInDelay),
           CreateOpacityElement(1.f, kGreetingAnimationFadeInDuration))});
}

void AssistantMainStage::OnPendingQueryChanged(const AssistantQuery& query) {
  // It is possible for the user to pend multiple queries in rapid succession.
  // When this happens, a new query can be pended before the previously
  // committed query was answered and activated. When this occurs, we discard
  // the view for the previously committed query as it has been aborted.
  if (committed_query_view_) {
    delete committed_query_view_;
    committed_query_view_ = nullptr;
  }

  if (!pending_query_view_) {
    pending_query_view_ = new AssistantQueryView();
    pending_query_view_->AddObserver(this);

    // The query view will be animated on its own layer.
    pending_query_view_->SetPaintToLayer();
    pending_query_view_->layer()->SetFillsBoundsOpaquely(false);

    query_layout_container_->AddChildView(pending_query_view_);

    // Starting from 0% opacity...
    pending_query_view_->layer()->SetOpacity(0.f);

    // ...animate the pending query view to 100% opacity.
    pending_query_view_->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
            CreateOpacityElement(1.f, kPendingQueryAnimationFadeInDuration)));

    UpdateFooter();
  }

  pending_query_view_->SetQuery(query);
}

void AssistantMainStage::OnPendingQueryCleared(bool due_to_commit) {
  if (pending_query_view_) {
    delete pending_query_view_;
    pending_query_view_ = nullptr;
  }

  // If the pending query is cleared but a committed query exists, we don't
  // need to update the footer because the footer visibility state should not
  // have changed.
  if (!committed_query_view_)
    UpdateFooter();
}

void AssistantMainStage::OnResponseChanged(
    const scoped_refptr<AssistantResponse>& response) {
  // Animate the progress indicator to 0% opacity.
  progress_indicator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          CreateOpacityElement(0.f, kProgressAnimationFadeOutDuration)));

  // We only consider the query active once the response has been received.
  OnActivateQuery();
}

void AssistantMainStage::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (assistant::util::IsStartingSession(new_visibility, old_visibility)) {
    // When Assistant is starting a new session, we animate in the appearance of
    // the greeting label and footer.

    AnimateInGreetingLabel();

    // Set up our pre-animation values.
    footer_->layer()->SetOpacity(0.f);

    const AssistantQuery& pending_query =
        delegate_->GetInteractionModel()->pending_query();

    // We only animate in the footer when a pending query is absent. Otherwise
    // the footer should be hidden to make room for the pending query view.
    if (pending_query.type() == AssistantQueryType::kNull) {
      // Animate the footer to 100% opacity with delay.
      footer_->layer()->GetAnimator()->StartAnimation(
          CreateLayerAnimationSequence(
              ui::LayerAnimationElement::CreatePauseElement(
                  ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                  kFooterEntryAnimationFadeInDelay),
              CreateOpacityElement(1.f, kFooterEntryAnimationFadeInDuration)));
    } else {
      // A pending query is present so we simulate a change event to synchronize
      // view state with interaction model state.
      OnPendingQueryChanged(pending_query);
    }

    return;
  }

  if (!assistant::util::IsFinishingSession(new_visibility))
    return;

  // When Assistant is finishing a session, we need to reset view state.
  is_first_query_ = true;

  delete active_query_view_;
  active_query_view_ = nullptr;

  delete committed_query_view_;
  committed_query_view_ = nullptr;

  delete pending_query_view_;
  pending_query_view_ = nullptr;

  greeting_label_->layer()->SetOpacity(1.f);
  greeting_label_->SetVisible(true);

  progress_indicator_->layer()->SetOpacity(0.f);
  progress_indicator_->layer()->SetTransform(gfx::Transform());

  UpdateTopPadding();
  UpdateFooter();
}

void AssistantMainStage::UpdateTopPadding() {
  // We need to apply top padding to the content and overlay layout containers
  // to reserve space for the active query view.
  const int top_padding = active_query_view_ ? active_query_view_->height() : 0;

  // Apply top padding to the content layout container by applying an empty
  // border to the header, its first child.
  header_->SetBorder(views::CreateEmptyBorder(top_padding, 0, 0, 0));

  // Force a layout/paint pass.
  content_layout_container_->Layout();
  content_layout_container_->SchedulePaint();

  // Apply top padding to the overlay layout container by applying an empty
  // border to its children.
  greeting_label_->SetBorder(views::CreateEmptyBorder(
      kGreetingLabelMarginTopDip + top_padding, 0, 0, 0));
  progress_indicator_->SetBorder(views::CreateEmptyBorder(
      kProgressIndicatorMarginTopDip + top_padding, 0, 0, 0));

  // Force a layout/paint pass.
  overlay_layout_container_->Layout();
  overlay_layout_container_->SchedulePaint();

  // Modifying borders may affect our preferred size.
  PreferredSizeChanged();
}

void AssistantMainStage::UpdateQueryViewTransform(views::View* query_view) {
  // Unless activated, a view for a query should be bottom aligned in its
  // parent. We calculate a top offset and apply a transformation to the query
  // view to have that effect and animate the transformation back to identity
  // when the query is activated on delivery of its response.
  DCHECK_NE(query_view, active_query_view_);

  const int top_offset =
      query_layout_container_->height() - query_view->height();

  gfx::Transform transform;
  transform.Translate(0, top_offset);
  query_view->layer()->SetTransform(transform);
}

void AssistantMainStage::UpdateFooter() {
  // The footer is only visible when the committed/pending query views are not.
  // When it is not visible, it should not process events.
  bool visible = !committed_query_view_ && !pending_query_view_;

  // Reset visibility to enable animation.
  footer_->SetVisible(true);

  if (visible) {
    // The footer will animate up into position so we need to set an initial
    // offset transformation from which to animate.
    gfx::Transform transform;
    transform.Translate(0, kFooterAnimationTranslationDip);
    footer_->layer()->SetTransform(transform);

    // Animate the entry of the footer.
    StartLayerAnimationSequencesTogether(
        footer_->layer()->GetAnimator(),
        {// Animate the translation with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
                 kFooterAnimationTranslationDelay),
             CreateTransformElement(gfx::Transform(),
                                    kFooterAnimationTranslationDuration,
                                    gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
         // Animate the fade in with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                 kFooterAnimationFadeInDelay),
             CreateOpacityElement(1.f, kFooterAnimationFadeInDuration))},
        // Observer animation start/end events.
        footer_animation_observer_.get());
  } else {
    // Animate the exit of the footer.
    StartLayerAnimationSequence(
        footer_->layer()->GetAnimator(),
        // Animate fade out.
        CreateLayerAnimationSequence(
            CreateOpacityElement(0.f, kFooterAnimationFadeOutDuration)),
        // Observe animation start/end events.
        footer_animation_observer_.get());
  }

  // Set the observer to active so that we'll receive start/end events.
  footer_animation_observer_->SetActive();
}

void AssistantMainStage::OnFooterAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The footer should not process events while animating.
  footer_->set_can_process_events_within_subtree(false);
}

bool AssistantMainStage::OnFooterAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The footer should only process events when visible. It is only visible when
  // there is no committed or pending query view.
  bool visible = !committed_query_view_ && !pending_query_view_;
  footer_->set_can_process_events_within_subtree(visible);
  footer_->SetVisible(visible);

  // Return false so that the observer does not destroy itself.
  return false;
}

}  // namespace ash
