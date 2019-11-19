// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_main_stage.h"

#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/base/stack_layout.h"
#include "ash/assistant/ui/main_stage/assistant_footer_view.h"
#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
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

// Appearance.
constexpr int kSeparatorThicknessDip = 1;
constexpr int kSeparatorWidthDip = 64;

// Footer entry animation.
constexpr base::TimeDelta kFooterEntryAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(283);
constexpr base::TimeDelta kFooterEntryAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);

// Divider animation.
constexpr base::TimeDelta kDividerAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(233);
constexpr base::TimeDelta kDividerAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kDividerAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);

// Greeting animation.
constexpr base::TimeDelta kGreetingAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr int kGreetingAnimationTranslationDip = 115;
constexpr base::TimeDelta kGreetingAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(33);
constexpr base::TimeDelta kGreetingAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kGreetingAnimationTranslateUpDuration =
    base::TimeDelta::FromMilliseconds(250);

// HorizontalSeparator ---------------------------------------------------------

// A horizontal line to separate the dialog plate.
class HorizontalSeparator : public views::View {
 public:
  explicit HorizontalSeparator(int preferred_width, int preferred_height)
      : preferred_width_(preferred_width),
        preferred_height_(preferred_height) {}

  ~HorizontalSeparator() override = default;

  // views::View overrides:
  const char* GetClassName() const override { return "HorizontalSeparator"; }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(preferred_width_, preferred_height_);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect draw_bounds(GetContentsBounds());
    draw_bounds.Inset(0, (draw_bounds.height() - kSeparatorThicknessDip) / 2);
    canvas->FillRect(draw_bounds, gfx::kGoogleGrey300);
  }

 private:
  const int preferred_width_;
  const int preferred_height_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalSeparator);
};

// A view is considered shown when it is visible and not in the process of
// fading out.
bool IsShown(const views::View* view) {
  DCHECK(view->layer());
  bool is_fading_out =
      cc::MathUtil::IsWithinEpsilon(view->layer()->GetTargetOpacity(), 0.f);

  return view->GetVisible() && !is_fading_out;
}

}  // namespace

// AppListAssistantMainStage ---------------------------------------------------

AppListAssistantMainStage::AppListAssistantMainStage(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kMainStage);
  InitLayout();

  // The view hierarchy will be destructed before AssistantController in Shell,
  // which owns AssistantViewDelegate, so AssistantViewDelegate is guaranteed to
  // outlive the AppListAssistantMainStage.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

AppListAssistantMainStage::~AppListAssistantMainStage() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* AppListAssistantMainStage::GetClassName() const {
  return "AppListAssistantMainStage";
}

void AppListAssistantMainStage::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AppListAssistantMainStage::OnViewPreferredSizeChanged(views::View* view) {
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void AppListAssistantMainStage::InitLayout() {
  // The children of AppListAssistantMainStage will be animated on their own
  // layers and we want them to be clipped by their parent layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* content_layout_container = CreateContentLayoutContainer();
  AddChildView(content_layout_container);
  layout->SetFlexForView(content_layout_container, 1);

  AddChildView(CreateFooterLayoutContainer());
}

views::View* AppListAssistantMainStage::CreateContentLayoutContainer() {
  // The content layout container stacks two views.
  // On top is a main content container including the line separator, progress
  // indicator query view and |ui_element_container_|.
  // |greeting_label_| is laid out beneath of the main content container. As
  // such, it appears underneath and does not cause repositioning to any of
  // content layout's underlying views.
  views::View* content_layout_container = new views::View();

  InitGreetingLabel();
  content_layout_container->AddChildView(greeting_label_);
  auto* stack_layout = content_layout_container->SetLayoutManager(
      std::make_unique<StackLayout>());

  // We need to stretch |greeting_label_| to match its parent so that it
  // won't use heuristics in Label to infer line breaking, which seems to cause
  // text clipping with DPI adjustment. See b/112843496.
  stack_layout->SetRespectDimensionForView(
      greeting_label_, StackLayout::RespectDimension::kHeight);
  stack_layout->SetVerticalAlignmentForView(
      greeting_label_, StackLayout::VerticalAlignment::kCenter);

  auto* main_content_layout_container = CreateMainContentLayoutContainer();
  content_layout_container->AddChildView(main_content_layout_container);

  // Do not respect height, otherwise bounds will not be set correctly for
  // scrolling.
  stack_layout->SetRespectDimensionForView(
      main_content_layout_container, StackLayout::RespectDimension::kWidth);

  return content_layout_container;
}

void AppListAssistantMainStage::InitGreetingLabel() {
  // Greeting label, which will be animated on its own layer.
  greeting_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
  greeting_label_->SetID(AssistantViewID::kGreetingLabel);
  greeting_label_->SetAutoColorReadabilityEnabled(false);
  greeting_label_->SetEnabledColor(kTextColorPrimary);
  greeting_label_->SetFontList(
      assistant::ui::GetDefaultFontList()
          .DeriveWithSizeDelta(8)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  greeting_label_->SetMultiLine(true);
  greeting_label_->SetPaintToLayer();
  greeting_label_->layer()->SetFillsBoundsOpaquely(false);
}

views::View* AppListAssistantMainStage::CreateMainContentLayoutContainer() {
  views::View* content_layout_container = new views::View();
  views::BoxLayout* content_layout = content_layout_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  content_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  content_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  content_layout_container->AddChildView(CreateDividerLayoutContainer());

  // Query view. Will be animated on its own layer.
  query_view_ = new AssistantQueryView();
  query_view_->SetPaintToLayer();
  query_view_->layer()->SetFillsBoundsOpaquely(false);
  query_view_->AddObserver(this);
  content_layout_container->AddChildView(query_view_);

  // UI element container.
  ui_element_container_ = new UiElementContainerView(delegate_);
  ui_element_container_->AddObserver(this);
  content_layout_container->AddChildView(ui_element_container_);
  content_layout->SetFlexForView(ui_element_container_, 1,
                                 /*use_min_size=*/true);

  return content_layout_container;
}

views::View* AppListAssistantMainStage::CreateDividerLayoutContainer() {
  // Dividers: the progress indicator and the horizontal separator will be the
  // separator when querying and showing the results, respectively.
  views::View* divider_container = new views::View();
  divider_container->SetLayoutManager(std::make_unique<StackLayout>());

  // Progress indicator, which will be animated on its own layer.
  progress_indicator_ = new AssistantProgressIndicator();
  progress_indicator_->SetPaintToLayer();
  progress_indicator_->layer()->SetFillsBoundsOpaquely(false);
  divider_container->AddChildView(progress_indicator_);

  // Horizontal separator, which will be animated on its own layer.
  horizontal_separator_ = new HorizontalSeparator(
      kSeparatorWidthDip, progress_indicator_->GetPreferredSize().height());
  horizontal_separator_->SetPaintToLayer();
  horizontal_separator_->layer()->SetFillsBoundsOpaquely(false);
  divider_container->AddChildView(horizontal_separator_);

  return divider_container;
}

views::View* AppListAssistantMainStage::CreateFooterLayoutContainer() {
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

  return footer_container;
}

void AppListAssistantMainStage::AnimateInGreetingLabel() {
  greeting_label_->layer()->GetAnimator()->StopAnimating();

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

void AppListAssistantMainStage::AnimateInFooter() {
  // Set up our pre-animation values.
  footer_->layer()->SetOpacity(0.f);

  // Animate the footer to 100% opacity with delay.
  footer_->layer()->GetAnimator()->StartAnimation(CreateLayerAnimationSequence(
      ui::LayerAnimationElement::CreatePauseElement(
          ui::LayerAnimationElement::AnimatableProperty::OPACITY,
          kFooterEntryAnimationFadeInDelay),
      CreateOpacityElement(1.f, kFooterEntryAnimationFadeInDuration)));
}

void AppListAssistantMainStage::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  // Update the view.
  query_view_->SetQuery(query);

  // If query is empty and we are showing greeting label, do not update the Ui.
  if (query.Empty() && IsShown(greeting_label_))
    return;

  // Hide the horizontal separator.
  horizontal_separator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          CreateOpacityElement(0.f, kDividerAnimationFadeOutDuration)));

  // Show the progress indicator.
  progress_indicator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          // Delay...
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kDividerAnimationFadeInDelay),
          // ...then fade in.
          CreateOpacityElement(1.f, kDividerAnimationFadeInDuration)));

  MaybeHideGreetingLabel();
}

void AppListAssistantMainStage::OnPendingQueryChanged(
    const AssistantQuery& query) {
  // Update the view.
  query_view_->SetQuery(query);

  if (!IsShown(greeting_label_))
    return;

  // Animate the opacity to 100% with delay equal to |greeting_label_| fade out
  // animation duration to avoid the two views displaying at the same time.
  constexpr base::TimeDelta kQueryAnimationFadeInDuration =
      base::TimeDelta::FromMilliseconds(433);
  query_view_->layer()->SetOpacity(0.f);
  query_view_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kGreetingAnimationFadeOutDuration),
          CreateOpacityElement(1.f, kQueryAnimationFadeInDuration)));

  if (!query.Empty())
    MaybeHideGreetingLabel();
}

void AppListAssistantMainStage::OnPendingQueryCleared(bool due_to_commit) {
  // When a pending query is cleared, it may be because the interaction was
  // cancelled, or because the query was committed. If the query was committed,
  // reseting the query here will have no visible effect. If the interaction was
  // cancelled, we set the query here to restore the previously committed query.
  query_view_->SetQuery(delegate_->GetInteractionModel()->committed_query());
}

void AppListAssistantMainStage::OnResponseChanged(
    const scoped_refptr<AssistantResponse>& response) {
  MaybeHideGreetingLabel();

  // Show the horizontal separator.
  horizontal_separator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          // Delay...
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kDividerAnimationFadeInDelay),
          // ...then fade in.
          CreateOpacityElement(1.f, kDividerAnimationFadeInDuration)));

  // Hide the progress indicator.
  progress_indicator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          CreateOpacityElement(0.f, kDividerAnimationFadeOutDuration)));
}

void AppListAssistantMainStage::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (assistant::util::IsStartingSession(new_visibility, old_visibility)) {
    // When Assistant is starting a new session, we animate in the appearance of
    // the greeting label and footer.
    const bool from_search =
        entry_point == AssistantEntryPoint::kLauncherSearchResult;
    progress_indicator_->layer()->SetOpacity(0.f);
    horizontal_separator_->layer()->SetOpacity(from_search ? 1.f : 0.f);

    if (!from_search)
      AnimateInGreetingLabel();
    else
      greeting_label_->SetVisible(false);

    AnimateInFooter();
    return;
  }

  query_view_->SetQuery(AssistantNullQuery());

  footer_->SetVisible(true);
  footer_->layer()->SetOpacity(1.f);
  footer_->set_can_process_events_within_subtree(true);
}

void AppListAssistantMainStage::MaybeHideGreetingLabel() {
  if (!IsShown(greeting_label_))
    return;

  assistant::util::FadeOutAndHide(greeting_label_,
                                  kGreetingAnimationFadeOutDuration);
}

}  // namespace ash
