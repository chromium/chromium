// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_view.h"

#include <memory>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_utils.h"

namespace ash {

namespace {

// References to the icons that correspond to different volume levels.
const gfx::VectorIcon* const kVolumeLevelIcons[] = {
    &kUnifiedMenuVolumeLowIcon,     // Low volume.
    &kUnifiedMenuVolumeMediumIcon,  // Medium volume.
    &kUnifiedMenuVolumeHighIcon,    // High volume.
    &kUnifiedMenuVolumeHighIcon,    // Full volume.
};

// The maximum index of kVolumeLevelIcons.
constexpr int kVolumeLevels = std::size(kVolumeLevelIcons) - 1;

// Get vector icon reference that corresponds to the given volume level. |level|
// is between 0.0 to 1.0.
const gfx::VectorIcon& GetVolumeIconForLevel(float level) {
  int index = static_cast<int>(std::ceil(level * kVolumeLevels));
  if (index < 0)
    index = 0;
  else if (index > kVolumeLevels)
    index = kVolumeLevels;
  return *kVolumeLevelIcons[index];
}

}  // namespace

UnifiedVolumeView::UnifiedVolumeView(
    UnifiedVolumeSliderController* controller,
    UnifiedVolumeSliderController::Delegate* delegate)
    : UnifiedSliderView(base::BindRepeating(
                            &UnifiedVolumeSliderController::SliderButtonPressed,
                            base::Unretained(controller)),
                        controller,
                        kSystemMenuVolumeHighIcon,
                        IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_LABEL),
      more_button_(AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&UnifiedVolumeSliderController::Delegate::
                                  OnAudioSettingsButtonClicked,
                              delegate->weak_ptr_factory_.GetWeakPtr()),
          IconButton::Type::kSmall,
          &kQuickSettingsRightArrowIcon,
          IDS_ASH_STATUS_TRAY_AUDIO))) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  Update(false /* by_user */);
}

UnifiedVolumeView::~UnifiedVolumeView() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

const char* UnifiedVolumeView::GetClassName() const {
  return "UnifiedVolumeView";
}

void UnifiedVolumeView::Update(bool by_user) {
  bool is_muted = CrasAudioHandler::Get()->IsOutputMuted();
  float level = CrasAudioHandler::Get()->GetOutputVolumePercent() / 100.f;

  // To indicate that the volume is muted, set the volume slider to the minimal
  // visual style.
  slider()->SetRenderingStyle(
      is_muted ? views::Slider::RenderingStyle::kMinimalStyle
               : views::Slider::RenderingStyle::kDefaultStyle);
  slider()->SetEnabled(!CrasAudioHandler::Get()->IsOutputMutedByPolicy());

  // The button should be gray when muted and colored otherwise.
  button()->SetToggled(!is_muted);
  button()->SetVectorIcon(is_muted ? kUnifiedMenuVolumeMuteIcon
                                   : GetVolumeIconForLevel(level));
  std::u16string state_tooltip_text = l10n_util::GetStringUTF16(
      is_muted ? IDS_ASH_STATUS_TRAY_VOLUME_STATE_MUTED
               : IDS_ASH_STATUS_TRAY_VOLUME_STATE_ON);
  button()->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_VOLUME, state_tooltip_text));

  // Slider's value is in finer granularity than audio volume level(0.01),
  // there will be a small discrepancy between slider's value and volume level
  // on audio side. To avoid the jittering in slider UI, use the slider's
  // current value.
  if (level != 1.0 && std::abs(level - slider()->GetValue()) <
                          kAudioSliderIgnoreUpdateThreshold) {
    level = slider()->GetValue();
  }
  // Note: even if the value does not change, we still need to call this
  // function to enable accessibility events (crbug.com/1013251).
  SetSliderValue(level, by_user);
}

void UnifiedVolumeView::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                  int volume) {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnOutputMuteChanged(bool mute_on) {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnAudioNodesChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnActiveOutputNodeChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnActiveInputNodeChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::ChildVisibilityChanged(views::View* child) {
  Layout();
}

}  // namespace ash
