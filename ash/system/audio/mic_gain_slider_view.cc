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

constexpr int kMicGainSliderViewSpacing = 8;

// Constants used in the revamped `AudioDetailedView`.
constexpr auto kQsMicGainSliderPadding = gfx::Insets::TLBR(0, 4, 0, 24);
constexpr auto kQsMicGainSliderViewPadding = gfx::Insets::TLBR(0, 20, 0, 0);

// Gets resource ID for the string that should be used for mute state portion of
// the microphone toggle button tooltip.
int GetMuteStateTooltipTextResourceId(bool is_muted,
                                      bool is_muted_by_mute_switch) {
  if (is_muted_by_mute_switch)
    return IDS_ASH_STATUS_TRAY_MIC_STATE_MUTED_BY_HW_SWITCH;
  if (is_muted)
    return IDS_ASH_STATUS_TRAY_MIC_STATE_MUTED;
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
        views::BoxLayout::Orientation::kHorizontal, kQsMicGainSliderViewPadding,
        kMicGainSliderViewSpacing));
    slider()->SetBorder(views::CreateEmptyBorder(kQsMicGainSliderPadding));
    layout->SetFlexForView(slider()->parent(), /*flex=*/1);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    announcement_view_ = AddChildView(std::make_unique<views::View>());

    Update(/*by_user=*/false);

    return;
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kMicGainSliderViewPadding,
      kMicGainSliderViewSpacing));
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

  // For device that has dual internal mics, both the sliders in the
  // `AudioDetailedView` will be shown if one of the internal mic is the active
  // node. All other input nodes will be hidden.
  // For QsRevamp: we want to show the sliders for all the input nodes, so we
  // don't need this code block to hide the slider that is inactive and is not
  // one of the dual internal mics.
  if (!features::IsQsRevampEnabled()) {
    // If the device has dual internal mics and the internal mic shown in the ui
    // is a stub, we need to show this slider despite the `device_id_` not
    // matching the active input node.
    const bool show_internal_stub =
        internal_ && (active_device && active_device->IsInternalMic()) &&
        audio_handler->HasDualInternalMic();

    if (audio_handler->GetPrimaryActiveInputNode() != device_id_ &&
        !show_internal_stub) {
      SetVisible(false);
      return;
    }
  }

  SetVisible(true);
  const bool is_muted = audio_handler->IsInputMuted();
  const bool is_muted_by_mute_switch =
      audio_handler->input_muted_by_microphone_mute_switch();

  float level = audio_handler->GetInputGainPercent() / 100.f;

  // Gets the input gain for each device to draw each slider in
  // `AudioDetailedView`.
  if (features::IsQsRevampEnabled()) {
    // If the device cannot be found by `device_id_`, hides this view and early
    // returns to avoid a crash.
    if (!audio_handler->GetDeviceFromId(device_id_)) {
      SetVisible(false);
      return;
    }
    level = audio_handler->GetInputGainPercentForDevice(device_id_) / 100.f;
  }

  if (toast_label()) {
    toast_label()->SetText(
        l10n_util::GetStringUTF16(is_muted ? IDS_ASH_STATUS_AREA_TOAST_MIC_OFF
                                           : IDS_ASH_STATUS_AREA_TOAST_MIC_ON));
  }

  if (!features::IsQsRevampEnabled()) {
    // To indicate that the volume is muted, set the volume slider to the
    // minimal visual style.
    slider()->SetRenderingStyle(
        is_muted ? views::Slider::RenderingStyle::kMinimalStyle
                 : views::Slider::RenderingStyle::kDefaultStyle);

    // The button should be gray when muted and colored otherwise.
    button()->SetToggled(!is_muted);
    button()->SetEnabled(!is_muted_by_mute_switch);
    button()->SetVectorIcon(is_muted ? kMutedMicrophoneIcon
                                     : kImeMenuMicrophoneIcon);
    std::u16string state_tooltip_text = l10n_util::GetStringUTF16(
        GetMuteStateTooltipTextResourceId(is_muted, is_muted_by_mute_switch));
    button()->SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_MIC_GAIN, state_tooltip_text));
  } else {
    static_cast<QuickSettingsSlider*>(slider())->SetSliderStyle(
        active_device_id != device_id_
            ? QuickSettingsSlider::Style::kRadioInactive
            : QuickSettingsSlider::Style::kRadioActive);

    slider_icon()->SetImage(ui::ImageModel::FromVectorIcon(
        is_muted ? kMutedMicrophoneIcon : kImeMenuMicrophoneIcon,
        active_device_id == device_id_
            ? cros_tokens::kCrosSysSystemOnPrimaryContainer
            : cros_tokens::kCrosSysSecondary,
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

BEGIN_METADATA(MicGainSliderView, views::View)
END_METADATA

}  // namespace ash
