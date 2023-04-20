// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/mic_gain_slider_view.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_slider.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
// Gets resource ID for the string that should be used for mute state portion of
// the microphone toggle button tooltip.
int GetMuteStateTooltipTextResourceId(bool is_muted,
                                      bool is_muted_by_mute_switch) {
  if (is_muted_by_mute_switch) {
    return IDS_ASH_STATUS_TRAY_MIC_STATE_MUTED_BY_HW_SWITCH;
  }
  if (is_muted) {
    return IDS_ASH_STATUS_TRAY_MIC_STATE_MUTED;
  }
  return IDS_ASH_STATUS_TRAY_MIC_STATE_ON;
}

}  // namespace

MicGainSliderView::MicGainSliderView(MicGainSliderController* controller)
    : UnifiedSliderView(
          base::BindRepeating(&MicGainSliderController::SliderButtonPressed,
                              base::Unretained(controller)),
          controller,
          kImeMenuMicrophoneIcon,
          IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_LABEL),
      device_id_(CrasAudioHandler::Get()->GetPrimaryActiveInputNode()),
      internal_(false) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  CreateToastLabel();
  slider()->SetVisible(false);
  announcement_view_ = AddChildView(std::make_unique<views::View>());
  Update(/*by_user=*/false);
  announcement_view_->GetViewAccessibility().AnnounceText(
      toast_label()->GetText());
}

MicGainSliderView::MicGainSliderView(MicGainSliderController* controller,
                                     uint64_t device_id,
                                     bool internal)
    : UnifiedSliderView(
          base::BindRepeating(&MicGainSliderController::SliderButtonPressed,
                              base::Unretained(controller)),
          controller,
          kImeMenuMicrophoneIcon,
          IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_LABEL,
          /*read_only=*/false,
          QuickSettingsSlider::Style::kRadioActive),
      device_id_(device_id),
      internal_(internal) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  if (features::IsQsRevampEnabled()) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kRadioSliderViewPadding,
        kSliderChildrenViewSpacing));
    slider()->SetBorder(views::CreateEmptyBorder(kRadioSliderPadding));
    slider()->SetPreferredSize(kRadioSliderPreferredSize);
    slider_icon()->SetBorder(views::CreateEmptyBorder(kRadioSliderIconPadding));
    layout->SetFlexForView(slider()->parent(), /*flex=*/1);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    announcement_view_ = AddChildView(std::make_unique<views::View>());
    SetPreferredSize(kRadioSliderPreferredSize);

    Update(/*by_user=*/false);

    return;
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kMicGainSliderViewPadding,
      kSliderChildrenViewSpacing));
  slider()->SetBorder(views::CreateEmptyBorder(kMicGainSliderPadding));
  layout->SetFlexForView(slider(), 1);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  announcement_view_ = AddChildView(std::make_unique<views::View>());

  Update(/*by_user=*/false);
}

MicGainSliderView::~MicGainSliderView() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void MicGainSliderView::Update(bool by_user) {
  auto* audio_handler = CrasAudioHandler::Get();
  uint64_t active_device_id = audio_handler->GetPrimaryActiveInputNode();
  auto* active_device = audio_handler->GetDeviceFromId(active_device_id);

  // For device that has dual internal mics, a new device will be created to
  // show only one slider for both the internal mics, and the new device has a
  // new id that doesn't match either of the internal mic id. If the device has
  // dual internal mics and the internal mic shown in the ui is a stub, we need
  // to show this slider despite the `device_id_` not matching the active input
  // node.
  const bool show_internal_stub =
      internal_ && (active_device && active_device->IsInternalMic()) &&
      audio_handler->HasDualInternalMic();

  // For QsRevamp: we want to show the sliders for all the input nodes, so we
  // don't need this code block to hide the slider that is inactive and is not
  // the newly created internal mic for the dual-internal-mic device.
  if (!features::IsQsRevampEnabled()) {
    if (audio_handler->GetPrimaryActiveInputNode() != device_id_ &&
        !show_internal_stub) {
      SetVisible(false);
      return;
    }
  }

  SetVisible(true);
  bool is_muted = audio_handler->IsInputMuted();
  const bool is_muted_by_mute_switch =
      audio_handler->input_muted_by_microphone_mute_switch();

  float level = audio_handler->GetInputGainPercent() / 100.f;

  // Gets the input gain for each device to draw each slider in
  // `AudioDetailedView`.
  if (features::IsQsRevampEnabled()) {
    uint64_t device_id;
    if (audio_handler->GetDeviceFromId(device_id_)) {
      // If the device can be found by its id, this slider must not be one of
      // the dual internal mic, thus the `device_id_` is the actual id for it.
      device_id = device_id_;
    } else if (show_internal_stub) {
      // If the device cannot be found by its id and the dual internal mic is
      // active, this slider must be the one for the dual internal mic, which is
      // the active input node.
      device_id = audio_handler->GetPrimaryActiveInputNode();
    } else {
      // If the device cannot be found by its id and the dual internal mic is
      // inactive, this slider must be the one for the dual internal mic, which
      // is currently inactive. Sets its level as the front internal mic level
      // by default.
      device_id =
          audio_handler->GetDeviceByType(AudioDeviceType::kFrontMic)->id;
    }

    // Checks if the device is muted. If so, sets its level to be 0 to render
    // the muted state for slider. Otherwise, gets its volume level.
    level =
        audio_handler->IsInputMutedForDevice(device_id)
            ? 0
            : audio_handler->GetInputGainPercentForDevice(device_id) / 100.f;
    is_muted = level == 0;
  }

  if (toast_label()) {
    toast_label()->SetText(
        l10n_util::GetStringUTF16(is_muted ? IDS_ASH_STATUS_AREA_TOAST_MIC_OFF
                                           : IDS_ASH_STATUS_AREA_TOAST_MIC_ON));
  }

  if (button()) {
    // The button should be gray when muted and colored otherwise.
    button()->SetToggled(!is_muted);
    button()->SetEnabled(!is_muted_by_mute_switch);
    button()->SetVectorIcon(is_muted ? kMutedMicrophoneIcon
                                     : kImeMenuMicrophoneIcon);
    std::u16string state_tooltip_text = l10n_util::GetStringUTF16(
        GetMuteStateTooltipTextResourceId(is_muted, is_muted_by_mute_switch));
    button()->SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_MIC_GAIN, state_tooltip_text));
  }

  if (!features::IsQsRevampEnabled()) {
    // To indicate that the volume is muted, set the volume slider to the
    // minimal visual style.
    slider()->SetRenderingStyle(
        is_muted ? views::Slider::RenderingStyle::kMinimalStyle
                 : views::Slider::RenderingStyle::kDefaultStyle);
  } else {
    // For active internal mic stub, `show_internal_stub` indicates whether it's
    // showing and `device_id_` doesn't match with `active_device_id`.
    const bool is_active = show_internal_stub || active_device_id == device_id_;
    static_cast<QuickSettingsSlider*>(slider())->SetSliderStyle(
        is_active ? QuickSettingsSlider::Style::kRadioActive
                  : QuickSettingsSlider::Style::kRadioInactive);

    slider_icon()->SetImage(ui::ImageModel::FromVectorIcon(
        is_muted ? kMutedMicrophoneIcon : kImeMenuMicrophoneIcon,
        is_active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                  : cros_tokens::kCrosSysOnSurfaceVariant,
        kQsSliderIconSize));
  }

  // Slider's value is in finer granularity than audio volume level(0.01),
  // there will be a small discrepancy between slider's value and volume level
  // on audio side. To avoid the jittering in slider UI, use the slider's
  // current value.
  if (std::abs(level - slider()->GetValue()) <
      kAudioSliderIgnoreUpdateThreshold) {
    level = slider()->GetValue();
  }
  // Note: even if the value does not change, we still need to call this
  // function to enable accessibility events (crbug.com/1013251).
  SetSliderValue(level, by_user);
}

void MicGainSliderView::OnInputNodeGainChanged(uint64_t node_id, int gain) {
  Update(/*by_user=*/true);
}

void MicGainSliderView::OnInputMuteChanged(
    bool mute_on,
    CrasAudioHandler::InputMuteChangeMethod method) {
  Update(/*by_user=*/true);
  announcement_view_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(mute_on ? IDS_ASH_STATUS_AREA_TOAST_MIC_OFF
                                        : IDS_ASH_STATUS_AREA_TOAST_MIC_ON));
}

void MicGainSliderView::OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) {
  Update(/*by_user=*/true);
}

void MicGainSliderView::OnActiveInputNodeChanged() {
  Update(/*by_user=*/true);
}

void MicGainSliderView::VisibilityChanged(View* starting_from,
                                          bool is_visible) {
  // Only trigger the visibility change when it's from parent as there are also
  // visibility changes in `Update()`.
  if (starting_from != this) {
    Update(/*by_user=*/true);
  }
}

BEGIN_METADATA(MicGainSliderView, views::View)
END_METADATA

}  // namespace ash
