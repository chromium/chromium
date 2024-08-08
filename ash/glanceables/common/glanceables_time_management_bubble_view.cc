// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"

#include "ash/glanceables/common/glanceables_contents_scroll_view.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/public/cpp/metrics_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// The interior margin should be 12, but space needs to be left for the focus in
// the child views.
constexpr int kTotalInteriorMargin = 12;
constexpr int kSpaceForFocusRing = 4;
constexpr int kInteriorGlanceableBubbleMargin =
    kTotalInteriorMargin - kSpaceForFocusRing;

constexpr int kScrollViewBottomMargin = 12;
constexpr int kListViewBetweenChildSpacing = 4;
constexpr gfx::Insets kFooterBorderInsets = gfx::Insets::TLBR(4, 6, 8, 2);

constexpr base::TimeDelta kExpandStateChangeAnimationDuration =
    base::Milliseconds(300);
constexpr base::TimeDelta kBubbleExpandAnimationDuration =
    base::Milliseconds(300);
constexpr base::TimeDelta kBubbleCollapseAnimationDuration =
    base::Milliseconds(250);
constexpr gfx::Tween::Type kBubbleAnimationTweenType =
    gfx::Tween::FAST_OUT_SLOW_IN;
constexpr gfx::Tween::Type kExpandStateChangeAnimationTweenType =
    gfx::Tween::ACCEL_5_70_DECEL_90;

}  // namespace

GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    GlanceablesExpandButton() = default;
GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    ~GlanceablesExpandButton() = default;

void GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    SetExpandedStateTooltipStringId(int tooltip_text_id) {
  expand_tooltip_string_id_ = tooltip_text_id;
  UpdateTooltip();
}

void GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    SetCollapsedStateTooltipStringId(int tooltip_text_id) {
  collapse_tooltip_string_id_ = tooltip_text_id;
  UpdateTooltip();
}

std::u16string GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    GetExpandedStateTooltipText() const {
  // The tooltip tells users that clicking on the button will collapse the
  // glanceables bubble.
  return collapse_tooltip_string_id_ == 0
             ? u""
             : l10n_util::GetStringUTF16(collapse_tooltip_string_id_);
}

std::u16string GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    GetCollapsedStateTooltipText() const {
  // The tooltip tells users that clicking on the button will expand the
  // glanceables bubble.
  return expand_tooltip_string_id_ == 0
             ? u""
             : l10n_util::GetStringUTF16(expand_tooltip_string_id_);
}

BEGIN_METADATA(GlanceablesTimeManagementBubbleView, GlanceablesExpandButton)
END_METADATA

GlanceablesTimeManagementBubbleView::ResizeAnimation::ResizeAnimation(
    int start_height,
    int end_height,
    gfx::AnimationDelegate* delegate,
    Type type)
    : gfx::LinearAnimation(delegate),
      type_(type),
      start_height_(start_height),
      end_height_(end_height) {
  base::TimeDelta duration;
  switch (type) {
    case Type::kContainerExpandStateChanged:
      duration = kExpandStateChangeAnimationDuration;
      break;
    case Type::kChildResize:
      duration = start_height > end_height ? kBubbleCollapseAnimationDuration
                                           : kBubbleExpandAnimationDuration;
      break;
  }
  SetDuration(duration *
              ui::ScopedAnimationDurationScaleMode::duration_multiplier());
}

int GlanceablesTimeManagementBubbleView::ResizeAnimation::GetCurrentHeight()
    const {
  auto tween_type = type_ == Type::kChildResize
                        ? kBubbleAnimationTweenType
                        : kExpandStateChangeAnimationTweenType;
  return gfx::Tween::IntValueBetween(
      gfx::Tween::CalculateValue(tween_type, GetCurrentValue()), start_height_,
      end_height_);
}

GlanceablesTimeManagementBubbleView::GlanceablesTimeManagementBubbleView(
    Context context) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);

  UpdateInteriorMargin();
  SetOrientation(views::LayoutOrientation::kVertical);

  auto* header_container =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  header_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_container->SetInteriorMargin(gfx::Insets::TLBR(
      kSpaceForFocusRing, kSpaceForFocusRing, 0, kSpaceForFocusRing));

  header_view_ =
      header_container->AddChildView(std::make_unique<views::FlexLayoutView>());
  header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_view_->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTimeManagementBubbleHeaderView));
  header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));

  expand_button_ = header_container->AddChildView(
      std::make_unique<GlanceablesExpandButton>());
  expand_button_->SetID(base::to_underlying(
      GlanceablesViewId::kTimeManagementBubbleExpandButton));
  // This is only set visible when both Tasks and Classroom exist, where the
  // elevated background is created in that case.
  expand_button_->SetVisible(false);
  expand_button_->SetCallback(base::BindRepeating(
      &GlanceablesTimeManagementBubbleView::ToggleExpandState,
      base::Unretained(this)));

  progress_bar_ = AddChildView(std::make_unique<GlanceablesProgressBarView>());
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  content_scroll_view_ =
      AddChildView(std::make_unique<GlanceablesContentsScrollView>(context));

  auto* const list_view = content_scroll_view_->SetContents(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetInsideBorderInsets(gfx::Insets::TLBR(0, kSpaceForFocusRing,
                                                   kScrollViewBottomMargin,
                                                   kSpaceForFocusRing))
          .SetBetweenChildSpacing(kListViewBetweenChildSpacing)
          .Build());

  items_container_view_ =
      list_view->AddChildView(std::make_unique<views::View>());
  items_container_view_->GetViewAccessibility().SetRole(ax::mojom::Role::kList);
  items_container_view_->SetID(base::to_underlying(
      GlanceablesViewId::kTimeManagementBubbleListContainer));
  items_container_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/gfx::Insets(), kListViewBetweenChildSpacing));

  list_footer_view_ = list_view->AddChildView(
      std::make_unique<GlanceablesListFooterView>(base::BindRepeating(
          &GlanceablesTimeManagementBubbleView::OnFooterButtonPressed,
          base::Unretained(this))));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTimeManagementBubbleListFooter));
  list_footer_view_->SetBorder(views::CreateEmptyBorder(kFooterBorderInsets));
  list_footer_view_->SetVisible(false);
}

GlanceablesTimeManagementBubbleView::~GlanceablesTimeManagementBubbleView() =
    default;

void GlanceablesTimeManagementBubbleView::ChildPreferredSizeChanged(
    View* child) {
  PreferredSizeChanged();
}

void GlanceablesTimeManagementBubbleView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  if (error_message_) {
    error_message_->UpdateBoundsToContainer(GetLocalBounds());
  }
}

void GlanceablesTimeManagementBubbleView::SetAnimationEndedClosureForTest(
    base::OnceClosure closure) {
  resize_animation_ended_closure_ = std::move(closure);
}

void GlanceablesTimeManagementBubbleView::SetUpResizeThroughputTracker(
    const std::string& histogram_name) {
  if (!GetWidget()) {
    return;
  }

  resize_throughput_tracker_.emplace(
      GetWidget()->GetCompositor()->RequestNewThroughputTracker());
  resize_throughput_tracker_->Start(
      ash::metrics_util::ForSmoothnessV3(base::BindRepeating(
          [](const std::string& histogram_name, int smoothness) {
            base::UmaHistogramPercentage(histogram_name, smoothness);
          },
          histogram_name)));
}

void GlanceablesTimeManagementBubbleView::MaybeDismissErrorMessage() {
  if (!error_message_.get()) {
    return;
  }

  RemoveChildViewT(std::exchange(error_message_, nullptr));
}

void GlanceablesTimeManagementBubbleView::ShowErrorMessage(
    const std::u16string& error_message,
    views::Button::PressedCallback callback,
    GlanceablesErrorMessageView::ButtonActionType type) {
  MaybeDismissErrorMessage();

  error_message_ = AddChildView(std::make_unique<GlanceablesErrorMessageView>(
      std::move(callback), error_message, type));
  error_message_->SetProperty(views::kViewIgnoredByLayoutKey, true);
}

gfx::Size GlanceablesTimeManagementBubbleView::GetMinimumSize() const {
  gfx::Size minimum_size = views::FlexLayoutView::GetMinimumSize();
  minimum_size.set_height(GetCollapsedStatePreferredHeight());
  return minimum_size;
}

gfx::Size GlanceablesTimeManagementBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // The animation was implemented to ignore `available_size`. See b/351880846
  // for more detail.
  const gfx::Size base_preferred_size =
      views::FlexLayoutView::CalculatePreferredSize({});

  if (resize_animation_) {
    return gfx::Size(base_preferred_size.width(),
                     resize_animation_->GetCurrentHeight());
  }

  return base_preferred_size;
}

void GlanceablesTimeManagementBubbleView::AnimationEnded(
    const gfx::Animation* animation) {
  if (resize_throughput_tracker_) {
    resize_throughput_tracker_->Stop();
    resize_throughput_tracker_.reset();
  }
  resize_animation_.reset();
  if (resize_animation_ended_closure_) {
    std::move(resize_animation_ended_closure_).Run();
  }

  PreferredSizeChanged();
}

void GlanceablesTimeManagementBubbleView::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void GlanceablesTimeManagementBubbleView::AnimationCanceled(
    const gfx::Animation* animation) {
  if (resize_throughput_tracker_) {
    resize_throughput_tracker_->Cancel();
    resize_throughput_tracker_.reset();
  }
  resize_animation_.reset();
  if (!resize_animation_ended_closure_.is_null()) {
    std::move(resize_animation_ended_closure_).Run();
  }
}

void GlanceablesTimeManagementBubbleView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GlanceablesTimeManagementBubbleView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GlanceablesTimeManagementBubbleView::CreateElevatedBackground() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBaseOpaque, 16.f));
  UpdateInteriorMargin();

  expand_button_->SetVisible(true);
  expand_button_->SetExpanded(is_expanded_);
  content_scroll_view_->SetOnOverscrollCallback(base::BindRepeating(
      &GlanceablesTimeManagementBubbleView::SetExpandState,
      base::Unretained(this),
      /*is_expanded=*/false, /*expand_by_overscroll=*/true));
}

void GlanceablesTimeManagementBubbleView::UpdateInteriorMargin() {
  const bool no_bottom_margin = !GetBackground() || is_expanded_;
  SetInteriorMargin(no_bottom_margin
                        ? gfx::Insets::TLBR(kInteriorGlanceableBubbleMargin,
                                            kInteriorGlanceableBubbleMargin, 0,
                                            kInteriorGlanceableBubbleMargin)
                        : gfx::Insets::TLBR(kInteriorGlanceableBubbleMargin,
                                            kInteriorGlanceableBubbleMargin,
                                            kTotalInteriorMargin,
                                            kInteriorGlanceableBubbleMargin));
}

void GlanceablesTimeManagementBubbleView::ToggleExpandState() {
  SetExpandState(!is_expanded_, /*expand_by_overscroll=*/false);
}

BEGIN_METADATA(GlanceablesTimeManagementBubbleView)
END_METADATA

}  // namespace ash
