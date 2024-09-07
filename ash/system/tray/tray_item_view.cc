// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_view.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/system/status_area_animation_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "base/metrics/histogram_functions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Animating in will start (after resize stage) when animation value is greater
// than this value.
constexpr double kAnimatingInStartValue = 0.5;

// Animating out will end (before resize stage) when animation value is less
// than this value.
constexpr double kAnimatingOutEndValue = 0.5;

constexpr char kShowAnimationSmoothnessHistogramName[] =
    "Ash.StatusArea.TrayItemView.Show";

constexpr char kHideAnimationSmoothnessHistogramName[] =
    "Ash.StatusArea.TrayItemView.Hide";

void RecordAnimationSmoothness(const std::string& histogram_name,
                               int smoothness) {
  DCHECK(0 <= smoothness && smoothness <= 100);
  base::UmaHistogramPercentage(histogram_name, smoothness);
}

void SetupThroughputTrackerForAnimationSmoothness(
    views::Widget* widget,
    std::optional<ui::ThroughputTracker>& tracker,
    const char* histogram_name) {
  // Return if `tracker` is already running; `widget` may not exist in tests.
  if (tracker || !widget)
    return;

  tracker.emplace(widget->GetCompositor()->RequestNewThroughputTracker());
  tracker->Start(ash::metrics_util::ForSmoothnessV3(
      base::BindRepeating(&RecordAnimationSmoothness, histogram_name)));
}

}  // namespace

void IconizedLabel::SetCustomAccessibleName(const std::u16string& name) {
  custom_accessible_name_ = name;

  UpdateAccessibleRole();
  GetViewAccessibility().SetName(custom_accessible_name_);
}

void IconizedLabel::AdjustAccessibleName(std::u16string& new_name,
                                         ax::mojom::NameFrom& name_from) {
  if (!custom_accessible_name_.empty()) {
    new_name = custom_accessible_name_;
    name_from = ax::mojom::NameFrom::kAttribute;
  } else {
    views::Label::AdjustAccessibleName(new_name, name_from);
  }
}

void IconizedLabel::UpdateAccessibleRole() {
  if (!custom_accessible_name_.empty()) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  } else {
    GetViewAccessibility().SetRole(GetTextContext() ==
                                           views::style::CONTEXT_DIALOG_TITLE
                                       ? ax::mojom::Role::kTitleBar
                                       : ax::mojom::Role::kStaticText);
  }
}

BEGIN_METADATA(IconizedLabel)
END_METADATA

TrayItemView::TrayItemView(Shelf* shelf)
    : views::AnimationDelegateViews(this), shelf_(shelf) {
  DCHECK(shelf_);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

TrayItemView::~TrayItemView() = default;

void TrayItemView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TrayItemView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TrayItemView::CreateLabel() {
  label_ = new IconizedLabel;
  AddChildView(label_.get());
  PreferredSizeChanged();
}

void TrayItemView::CreateImageView() {
  image_view_ = new views::ImageView;
  AddChildView(image_view_.get());
  PreferredSizeChanged();
}

void TrayItemView::DestroyLabel() {
  if (!label_)
    return;

  RemoveChildViewT(label_.get());
  label_ = nullptr;
}

void TrayItemView::DestroyImageView() {
  if (!image_view_)
    return;

  RemoveChildViewT(image_view_.get());
  image_view_ = nullptr;
}

void TrayItemView::UpdateLabelOrImageViewColor(bool active) {
  is_active_ = active;
}

base::ScopedClosureRunner TrayItemView::DisableAnimation() {
  if (layer()->GetAnimator()->is_animating()) {
    layer()->GetAnimator()->StopAnimating();
  }
  ++disable_animation_count_;
  return base::ScopedClosureRunner(base::BindOnce(
      [](const base::WeakPtr<TrayItemView>& ptr) {
        if (ptr) {
          --ptr->disable_animation_count_;
        }
      },
      weak_factory_.GetWeakPtr()));
}

void TrayItemView::SetAnimationIdleClosureForTest(base::OnceClosure closure) {
  animation_idle_closure_ = std::move(closure);
}

bool TrayItemView::IsAnimating() {
  return animation_ && animation_->is_animating();
}

void TrayItemView::SetVisible(bool visible) {
  // Do not invoke animation when the current visibility is already at the
  // target visibility.
  if (visible == target_visible_) {
    return;
  }
  target_visible_ = visible;
  for (auto& observer : observers_) {
    observer.OnTrayItemVisibilityAboutToChange(target_visible_);
  }
  views::View::SetVisible(visible);
  // During startup TrayItemViews are often SetVisible(false) before they are
  // attached to a widget. Don't bother constructing animations for them.
  if (!GetWidget()) {
    return;
  }
  PerformVisibilityAnimation(visible);
}

bool TrayItemView::IsHorizontalAlignment() const {
  return shelf_->IsHorizontalAlignment();
}

void TrayItemView::PerformVisibilityAnimation(bool visible) {
  // Set the view visible to show both show/hide animation.
  views::View::SetVisible(true);

  if (!animation_) {
    animation_ = std::make_unique<gfx::SlideAnimation>(this);
    animation_->SetTweenType(gfx::Tween::LINEAR);
    animation_->Reset(target_visible_ ? 0.0 : 1.0);
  }

  // Immediately progress to the end of the animation if animation is disabled.
  // NOTE: `ScreenRotationAnimator` can set animations to ZERO_DURATION.
  if (!ShouldVisibilityChangeBeAnimated() ||
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    // Tray items need to stay visible if the notification center tray's hide
    // animation is going to run, so don't hide the tray item here.
    // `StatusAreaAnimationController` will call `ImmediatelyUpdateVisibility()`
    // once the hide animation is over to ensure that all tray items are given a
    // chance to properly update their visibilities.
    if (!target_visible_ && shelf_->status_area_widget()
                                ->animation_controller()
                                ->is_hide_animation_scheduled()) {
      return;
    }
    animation_->SetSlideDuration(base::TimeDelta());
    target_visible_ ? animation_->Show() : animation_->Hide();
    return;
  }

  if (target_visible_) {
    SetupThroughputTrackerForAnimationSmoothness(
        GetWidget(), show_throughput_tracker_,
        kShowAnimationSmoothnessHistogramName);
    animation_->SetSlideDuration(base::Milliseconds(400));
    animation_->Show();
    AnimationProgressed(animation_.get());
  } else {
    SetupThroughputTrackerForAnimationSmoothness(
        GetWidget(), hide_throughput_tracker_,
        kHideAnimationSmoothnessHistogramName);
    animation_->SetSlideDuration(base::Milliseconds(100));
    animation_->Hide();
    AnimationProgressed(animation_.get());
  }
}

void TrayItemView::ImmediatelyUpdateVisibility() {
  // Reset the animation to the end state according to `target_visible_` so that
  // future visibility changes can animate properly.
  if (animation_) {
    animation_->Reset(target_visible_ ? 1.0 : 0.0);
  }
  layer()->SetTransform(gfx::Transform());
  layer()->SetOpacity(target_visible_ ? 1.0 : 0.0);
  views::View::SetVisible(target_visible_);
}

gfx::Size TrayItemView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  DCHECK_EQ(1u, children().size());
  gfx::Size size = views::View::CalculatePreferredSize(available_size);
  if (image_view_) {
    size = gfx::Size(kUnifiedTrayIconSize, kUnifiedTrayIconSize);
    // Some TrayItemViews have slightly larger icons (e.g. Ethernet with VPN
    // badge).
    size.SetToMax(image_view_->CalculatePreferredSize({}));
  }

  if (!animation_.get() || !animation_->is_animating() ||
      !InResizeAnimation(animation_->GetCurrentValue())) {
    return size;
  }

  double progress = gfx::Tween::CalculateValue(
      gfx::Tween::FAST_OUT_SLOW_IN,
      GetResizeProgressFromAnimationProgress(animation_->GetCurrentValue()));
  if (shelf_->IsHorizontalAlignment()) {
    size.set_width(std::max(1, static_cast<int>(size.width() * progress)));
  } else {
    size.set_height(std::max(1, static_cast<int>(size.height() * progress)));
  }
  return size;
}

void TrayItemView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayItemView::AnimationProgressed(const gfx::Animation* animation) {
  // Should not animate during resize stage.
  if (InResizeAnimation(animation->GetCurrentValue())) {
    // Ensure we are not visible during resize stage.
    if (layer()->opacity() > 0.0) {
      layer()->SetOpacity(0.0);
    }
    PreferredSizeChanged();
    return;
  }

  double scale_progress =
      GetItemScaleProgressFromAnimationProgress(animation->GetCurrentValue());
  layer()->SetOpacity(scale_progress);

  // Only scale when animating icon in.
  if (target_visible_ && use_scale_in_animation_) {
    scale_progress = gfx::Tween::CalculateValue(gfx::Tween::LINEAR_OUT_SLOW_IN,
                                                scale_progress);
    gfx::Transform transform;
    transform.Translate(
        gfx::Tween::DoubleValueBetween(scale_progress,
                                       static_cast<double>(width()) / 2, 0.),
        gfx::Tween::DoubleValueBetween(scale_progress,
                                       static_cast<double>(height()) / 2, 0.));
    transform.Scale(scale_progress, scale_progress);
    layer()->SetTransform(transform);
  }

  // Container size might not fully transition to full size (the resize progress
  // value converted from animation progress might not be 1 after resize
  // animation). This call makes sure that it is fully resized.
  PreferredSizeChanged();
}

void TrayItemView::AnimationEnded(const gfx::Animation* animation) {
  views::View::SetVisible(target_visible_);
  layer()->SetOpacity(target_visible_ ? 1.0 : 0.0);

  if (show_throughput_tracker_) {
    // Reset `show_throughput_tracker_` to reset animation metrics recording.
    show_throughput_tracker_->Stop();
    show_throughput_tracker_.reset();
  }

  if (hide_throughput_tracker_) {
    // Reset `hide_throughput_tracker_` to reset animation metrics recording.
    hide_throughput_tracker_->Stop();
    hide_throughput_tracker_.reset();
  }

  if (animation_idle_closure_) {
    std::move(animation_idle_closure_).Run();
  }
}

void TrayItemView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

bool TrayItemView::InResizeAnimation(double animation_value) const {
  // Animation should be delayed for the first part of animating in and last
  // part of animating out, allowing item resize happen before item animating in
  // and after item animating out.
  return ((target_visible_ && animation_value <= kAnimatingInStartValue) ||
          (!target_visible_ && animation_value <= kAnimatingOutEndValue));
}

double TrayItemView::GetResizeProgressFromAnimationProgress(
    double animation_value) const {
  DCHECK(InResizeAnimation(animation_value));
  // When animating in, convert value from [0,kAnimatingInStartValue] to [0,1].
  if (target_visible_)
    return animation_value * (1 / kAnimatingInStartValue);

  // When animating out, convert value from [kAnimatingOutEndValue,0] to [1,0].
  return animation_value * (1 / kAnimatingOutEndValue);
}

double TrayItemView::GetItemScaleProgressFromAnimationProgress(
    double animation_value) const {
  DCHECK(!InResizeAnimation(animation_value));
  // When animating in, convert value from [kAnimatingInStartValue,1] to [0,1].
  if (target_visible_) {
    return (animation_value - kAnimatingInStartValue) *
           (1 / (1 - kAnimatingInStartValue));
  }

  // When animating out, convert value from [1,kAnimatingOutEndValue] to [1,0].
  return (animation_value - kAnimatingOutEndValue) *
         (1 / (1 - kAnimatingOutEndValue));
}

BEGIN_METADATA(TrayItemView)
END_METADATA

}  // namespace ash
