// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/mic_gain_slider_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

using chromeos::CrasAudioHandler;

namespace ash {

MicGainSliderView::MicGainSliderView(MicGainSliderController* controller,
                                     uint64_t device_id,
                                     bool internal)
    : UnifiedSliderView(controller,
                        kImeMenuMicrophoneIcon,
                        IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_LABEL),
      device_id_(device_id),
      internal_(internal) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kMicGainSliderViewPadding,
      kMicGainSliderViewSpacing));
  slider()->SetBorder(views::CreateEmptyBorder(kMicGainSliderPadding));
  layout->SetFlexForView(slider(), 1);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  Update(false /* by_user */);
}

MicGainSliderView::~MicGainSliderView() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void MicGainSliderView::Update(bool by_user) {
  auto* audio_handler = CrasAudioHandler::Get();
  uint64_t active_device_id = audio_handler->GetPrimaryActiveInputNode();
  auto* active_device = audio_handler->GetDeviceFromId(active_device_id);

  // If the device has dual internal mics the internal mic shown in the ui is a
  // stub. We need to show this slider despite the device_id_ not matching the
  // active input node.
  bool show_internal_stub = internal_ &&
                            (active_device && active_device->IsInternalMic()) &&
                            audio_handler->HasDualInternalMic();

  if (audio_handler->GetPrimaryActiveInputNode() != device_id_ &&
      !show_internal_stub) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  bool is_muted = audio_handler->IsInputMuted();
  float level = audio_handler->GetInputGainPercent() / 100.f;
  // To indicate that the volume is muted, set the volume slider to the minimal
  // visual style.
  slider()->SetRenderingStyle(
      is_muted ? views::Slider::RenderingStyle::kMinimalStyle
               : views::Slider::RenderingStyle::kDefaultStyle);

  // The button should be gray when muted and colored otherwise.
  button()->SetToggled(!is_muted);
  button()->SetVectorIcon(is_muted ? kMutedMicrophoneIcon
                                   : kImeMenuMicrophoneIcon);
  base::string16 state_tooltip_text =
      l10n_util::GetStringUTF16(is_muted ? IDS_ASH_STATUS_TRAY_MIC_STATE_MUTED
                                         : IDS_ASH_STATUS_TRAY_MIC_STATE_ON);
  button()->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_MIC_GAIN, state_tooltip_text));

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
  Update(true /* by_user */);
}

void MicGainSliderView::OnInputMuteChanged(bool mute_on) {
  Update(true /* by_user */);
}

void MicGainSliderView::OnActiveInputNodeChanged() {
  Update(true /* by_user */);
}

const char* MicGainSliderView::GetClassName() const {
  return "MicGainSliderView";
}

}  // namespace ash
