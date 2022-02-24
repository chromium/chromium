// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_button_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {

// Animation.
constexpr base::TimeDelta kInProgressAnimationOpacityDuration =
    base::Milliseconds(100);
constexpr base::TimeDelta kInProgressAnimationScaleDelay =
    base::Milliseconds(50);
constexpr base::TimeDelta kInProgressAnimationScaleDuration =
    base::Milliseconds(166);
constexpr float kInProgressAnimationScaleFactor = 0.875f;

// Helper function that creates an image for the dictation icon.
gfx::ImageSkia GetIconImage(bool enabled) {
  const SkColor color =
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState());
  return enabled ? gfx::CreateVectorIcon(kDictationOnNewuiIcon, color)
                 : gfx::CreateVectorIcon(kDictationOffNewuiIcon, color);
}

}  // namespace

DictationButtonTray::DictationButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(new views::ImageView()),
      download_progress_(0) {
  const gfx::ImageSkia icon_image = GetIconImage(/*enabled=*/false);
  const int vertical_padding = (kTrayItemSize - icon_image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - icon_image.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION));
  tray_container()->AddChildView(icon_);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->accessibility_controller()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
}

DictationButtonTray::~DictationButtonTray() {
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

bool DictationButtonTray::PerformAction(const ui::Event& event) {
  Shell::Get()->accessibility_controller()->ToggleDictationFromSource(
      DictationToggleSource::kButton);

  CheckDictationStatusAndUpdateIcon();
  return true;
}

void DictationButtonTray::Initialize() {
  TrayBackgroundView::Initialize();
  UpdateVisibility();
}

void DictationButtonTray::ClickedOutsideBubble() {}

void DictationButtonTray::OnDictationStarted() {
  UpdateIcon(/*dictation_active=*/true);
}

void DictationButtonTray::OnDictationEnded() {
  UpdateIcon(/*dictation_active=*/false);
}

void DictationButtonTray::OnAccessibilityStatusChanged() {
  UpdateVisibility();
  CheckDictationStatusAndUpdateIcon();
}

std::u16string DictationButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_DICTATION_BUTTON_ACCESSIBLE_NAME);
}

void DictationButtonTray::HandleLocaleChange() {
  icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION));
}

void DictationButtonTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  // This class has no bubbles to hide.
}

void DictationButtonTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  icon_->SetImage(GetIconImage(
      Shell::Get()->accessibility_controller()->dictation_active()));
  if (progress_indicator_)
    progress_indicator_->InvalidateLayer();
}

void DictationButtonTray::Layout() {
  TrayBackgroundView::Layout();
  UpdateProgressIndicatorBounds();
}

const char* DictationButtonTray::GetClassName() const {
  return "DictationButtonTray";
}

void DictationButtonTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  CheckDictationStatusAndUpdateIcon();
}

void DictationButtonTray::UpdateIcon(bool dictation_active) {
  icon_->SetImage(GetIconImage(dictation_active));
  SetIsActive(dictation_active);
}

void DictationButtonTray::UpdateIconOpacityAndTransform() {
  // Updating the tray `icon_` opacity and transform is done to prevent overlap
  // with the inner icon of the `progress_indicator_` which is only present when
  // in-progress animation v2 is enabled.
  if (!features::IsHoldingSpaceInProgressAnimationV2Enabled() ||
      !progress_indicator_) {
    return;
  }

  // When `progress` is not `complete`, the `progress_indicator_` will paint an
  // inner icon in the same position as the tray `icon_`. To prevent overlap,
  // the tray `icon_` should be hidden when downloading is in `progress`.
  const absl::optional<float>& progress = progress_indicator_->progress();
  bool complete = progress == ProgressIndicator::kProgressComplete;
  float target_opacity = complete ? 1.f : 0.f;

  // Lazily create a `layer` for `icon_`.
  ui::Layer* layer = icon_->layer();
  if (!layer) {
    icon_->SetPaintToLayer();
    layer = icon_->layer();
    layer->SetFillsBoundsOpaquely(false);
  }

  // No-op if the tray `icon_` is already animating towards the desired state.
  if (layer->GetTargetOpacity() == target_opacity)
    return;

  // When the tray `icon_` should be hidden, it should be hidden immediately
  // without animation to prevent overlapping with the `progress_indicator_`'s
  // inner icon.
  if (target_opacity == 0.f) {
    layer->SetOpacity(0.f);
    return;
  }

  // When the tray `icon_` has not yet been laid out, it is not necessary to
  // animate it in. We can do so immediately.
  const gfx::Rect& bounds = icon_->bounds();
  if (bounds.IsEmpty()) {
    layer->SetOpacity(1.f);
    layer->SetTransform(gfx::Transform());
    return;
  }

  const auto preemption_strategy =
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET;
  const auto transform = gfx::GetScaleTransform(
      bounds.CenterPoint(), kInProgressAnimationScaleFactor);
  const auto tween_type = gfx::Tween::Type::FAST_OUT_SLOW_IN_3;

  // Animate the tray `icon_` from:
  // * Opacity: 0% -> 100%
  // * Scale: 87.5% -> 100%
  views::AnimationBuilder()
      .SetPreemptionStrategy(preemption_strategy)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 0.f)
      .SetTransform(layer, transform)
      .Then()
      .SetDuration(kInProgressAnimationOpacityDuration)
      .SetOpacity(layer, 1.f)
      .Offset(kInProgressAnimationScaleDelay)
      .SetDuration(kInProgressAnimationScaleDuration)
      .SetTransform(layer, gfx::Transform(), tween_type);
}

void DictationButtonTray::UpdateProgressIndicatorBounds() {
  if (progress_indicator_)
    progress_indicator_->layer()->SetBounds(GetBackgroundBounds());
}

void DictationButtonTray::UpdateVisibility() {
  bool is_visible =
      Shell::Get()->accessibility_controller()->dictation().enabled();
  SetVisiblePreferred(is_visible);
}

void DictationButtonTray::CheckDictationStatusAndUpdateIcon() {
  UpdateIcon(Shell::Get()->accessibility_controller()->dictation_active());
}

void DictationButtonTray::UpdateOnSpeechRecognitionDownloadChanged(
    int download_progress) {
  if (!visible_preferred())
    return;

  bool download_in_progress = download_progress > 0 && download_progress < 100;
  SetEnabled(!download_in_progress);
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      download_in_progress
          ? IDS_ASH_ACCESSIBILITY_DICTATION_BUTTON_TOOLTIP_SODA_DOWNLOADING
          : IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION));

  // Progress indicator.
  download_progress_ = download_progress;
  if (!progress_indicator_) {
    // A progress indicator that is only visible when a SODA download is
    // in-progress and a subscription to receive notification of progress
    // changed events.
    progress_indicator_ =
        ProgressIndicator::CreateDefaultInstance(base::BindRepeating(
            [](DictationButtonTray* tray) -> absl::optional<float> {
              // If download is in-progress, return the progress as a decimal.
              // Otherwise, the progress indicator shouldn't be painted.
              const int progress = tray->download_progress();
              return (progress > 0 && progress < 100)
                         ? progress / 100.f
                         : ProgressIndicator::kProgressComplete;
            },
            base::Unretained(this)));
    progress_changed_subscription_ =
        progress_indicator_->AddProgressChangedCallback(base::BindRepeating(
            &DictationButtonTray::UpdateIconOpacityAndTransform,
            base::Unretained(this)));
    layer()->Add(progress_indicator_->CreateLayer());
    UpdateProgressIndicatorBounds();
  }
  progress_indicator_->InvalidateLayer();
}

}  // namespace ash
