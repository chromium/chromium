// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"

#include "ash/public/cpp/metrics_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

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
    GlanceablesExpandButton(int expand_tooltip_string_id,
                            int collapse_tooltip_string_id)
    : expand_tooltip_string_id_(expand_tooltip_string_id),
      collapse_tooltip_string_id_(collapse_tooltip_string_id) {
  // Base class ctor doesn't have the tooltip string information yet. Explicitly
  // call `UpdateTooltip` to set it.
  UpdateTooltip();
}

GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    ~GlanceablesExpandButton() = default;

std::u16string GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    GetExpandedStateTooltipText() const {
  // The tooltip tells users that clicking on the button will collapse the
  // glanceables bubble.
  return l10n_util::GetStringUTF16(collapse_tooltip_string_id_);
}

std::u16string GlanceablesTimeManagementBubbleView::GlanceablesExpandButton::
    GetCollapsedStateTooltipText() const {
  // The tooltip tells users that clicking on the button will expand the
  // glanceables bubble.
  return l10n_util::GetStringUTF16(expand_tooltip_string_id_);
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

GlanceablesTimeManagementBubbleView::GlanceablesTimeManagementBubbleView() =
    default;
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

BEGIN_METADATA(GlanceablesTimeManagementBubbleView)
END_METADATA

}  // namespace ash
