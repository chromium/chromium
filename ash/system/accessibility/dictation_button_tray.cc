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
#include "ash/system/holding_space/holding_space_progress_indicator.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// Helper function that creates an image for the dictation icon.
gfx::ImageSkia GetIconImage(bool enabled) {
  const SkColor color =
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState());
  return enabled ? gfx::CreateVectorIcon(kDictationOnNewuiIcon, color)
                 : gfx::CreateVectorIcon(kDictationOffNewuiIcon, color);
}

DictationProgressIndicator::DictationProgressIndicator(
    const DictationButtonTray* tray)
    : HoldingSpaceProgressIndicator(/*animation_key=*/tray), tray_(tray) {}

bool DictationProgressIndicator::IsVisible() {
  absl::optional<float> progress = CalculateProgress();
  if (!progress.has_value())
    return false;

  if (progress.value() == 0.f || progress.value() == 1.f)
    return false;

  return true;
}

absl::optional<float> DictationProgressIndicator::CalculateProgress() const {
  int progress = tray_->download_progress();
  bool download_in_progress = progress > 0 && progress < 100;
  // If download is in-progress, return the progress as a decimal. Otherwise,
  // the progress indicator shouldn't be painted.
  return (download_in_progress)
             ? static_cast<double>(progress) / static_cast<double>(100)
             : HoldingSpaceProgressIndicator::kProgressComplete;
}

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
}

void DictationButtonTray::Layout() {
  TrayBackgroundView::Layout();
  if (progress_indicator_)
    progress_indicator_->layer()->SetBounds(GetBackgroundBounds());
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
  if (!::features::IsExperimentalAccessibilityDictationOfflineEnabled() ||
      !visible_preferred())
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
    // in-progress.
    progress_indicator_ = std::make_unique<DictationProgressIndicator>(this);
    layer()->Add(progress_indicator_->layer());
  }
  progress_indicator_->InvalidateLayer();
}

}  // namespace ash
