// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/audio/unified_volume_view.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_slider.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

using Style = QuickSettingsSlider::Style;

UnifiedVolumeView::UnifiedVolumeView(
    UnifiedVolumeSliderController* controller,
    UnifiedVolumeSliderController::Delegate* delegate,
    bool is_active_output_node)
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
          IconButton::Type::kMediumFloating,
          &kQuickSettingsRightArrowIcon,
          IDS_ASH_STATUS_TRAY_AUDIO))),
      is_active_output_node_(is_active_output_node),
      device_id_(CrasAudioHandler::Get()->GetPrimaryActiveOutputNode()) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  // In the case that there is a trusted pinned window (fullscreen lock mode)
  // and the volume slider popup is shown, do not allow the more_button_ to
  // open quick settings.
  auto* window = Shell::Get()->screen_pinning_controller()->pinned_window();
  if (window && WindowState::Get(window)->IsTrustedPinned()) {
    more_button_->SetEnabled(false);
  }

  more_button_->SetIconColor(cros_tokens::kCrosSysSecondary);
  more_button_->SetProperty(views::kElementIdentifierKey,
                            kQuickSettingsAudioDetailedViewButtonElementId);

  // TODO(b/257151067): Update the a11y name id.
  // Adds the live caption button before `more_button_`.
  Shell::Get()->accessibility_controller()->AddObserver(this);
  const bool enabled =
      Shell::Get()->accessibility_controller()->live_caption().enabled();
  live_caption_button_ = AddChildViewAt(
      std::make_unique<IconButton>(
          base::BindRepeating(&UnifiedVolumeView::OnLiveCaptionButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kMedium,
          enabled ? &kUnifiedMenuLiveCaptionIcon
                  : &kUnifiedMenuLiveCaptionOffIcon,
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP,
              l10n_util::GetStringUTF16(
                  enabled
                      ? IDS_ASH_STATUS_TRAY_LIVE_CAPTION_ENABLED_STATE_TOOLTIP
                      : IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP)),
          /*is_togglable=*/true,
          /*has_border=*/true),
      GetIndexOf(more_button_).value());
  // Sets the icon, icon color, background color for `live_caption_button_`
  // when it's toggled.
  live_caption_button_->SetToggledVectorIcon(kUnifiedMenuLiveCaptionIcon);
  live_caption_button_->SetIconToggledColor(
      cros_tokens::kCrosSysSystemOnPrimaryContainer);
  live_caption_button_->SetBackgroundToggledColor(
      cros_tokens::kCrosSysSystemPrimaryContainer);
  // Sets the icon, icon color, background color for `live_caption_button_`
  // when it's not toggled.
  live_caption_button_->SetVectorIcon(kUnifiedMenuLiveCaptionOffIcon);
  live_caption_button_->SetIconColor(cros_tokens::kCrosSysOnSurface);
  live_caption_button_->SetBackgroundColor(cros_tokens::kCrosSysSystemOnBase);

  live_caption_button_->SetToggled(enabled);

  Update(/*by_user=*/false);
}

UnifiedVolumeView::UnifiedVolumeView(UnifiedVolumeSliderController* controller,
                                     uint64_t device_id,
                                     bool is_active_output_node,
                                     const gfx::Insets& inside_padding)
    : UnifiedSliderView(base::BindRepeating(
                            &UnifiedVolumeSliderController::SliderButtonPressed,
                            base::Unretained(controller)),
                        controller,
                        kSystemMenuVolumeHighIcon,
                        IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_LABEL,
                        /*is_togglable=*/true,
                        /*read_only=*/false,
                        Style::kRadioActive),
      more_button_(nullptr),
      is_active_output_node_(is_active_output_node),
      device_id_(device_id) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, inside_padding,
      kSliderChildrenViewSpacing));
  slider()->SetBorder(views::CreateEmptyBorder(kRadioSliderPadding));
  slider()->SetPreferredSize(kRadioSliderPreferredSize);
  slider_button()->SetBorder(views::CreateEmptyBorder(kRadioSliderIconPadding));
  layout->SetFlexForView(slider(), /*flex=*/1);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  Update(/*by_user=*/false);
}

UnifiedVolumeView::~UnifiedVolumeView() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void UnifiedVolumeView::Update(bool by_user) {
  auto* audio_handler = CrasAudioHandler::Get();
  float level = audio_handler->GetOutputVolumePercent() / 100.f;
  level = audio_handler->GetOutputVolumePercentForDevice(device_id_) / 100.f;

  // Still needs to check if `level` is 0 because toggling the output mute by
  // keyboard will not set it to be muted in `UnifiedVolumeSliderController`.
  const bool is_muted =
      audio_handler->IsOutputMutedForDevice(device_id_) || level == 0;

  auto active_device_id = audio_handler->GetPrimaryActiveOutputNode();

  // Updates the style before updating the slider icon.
  auto* qs_slider = static_cast<QuickSettingsSlider*>(slider());
  const Style slider_style = qs_slider->slider_style();
  // The default style is for the slider in the main page and in the toast.
  const bool is_default_style =
      (slider_style == Style::kDefault || slider_style == Style::kDefaultMuted);
  if (is_default_style) {
    qs_slider->SetSliderStyle(is_muted ? Style::kDefaultMuted
                                       : Style::kDefault);
  } else if (active_device_id == device_id_) {
    qs_slider->SetSliderStyle(is_muted ? Style::kRadioActiveMuted
                                       : Style::kRadioActive);
  }

  // Updates the slider icon based on mute state and the style for radio
  // sliders.
  switch (slider_style) {
    case Style::kDefault:
    case Style::kDefaultMuted: {
      // TODO(b/257151067): Adds tooltip.
      slider_button()->SetVectorIcon(is_muted ? kUnifiedMenuVolumeMuteIcon
                                              : GetVolumeIconForLevel(level));
      slider_button()->SetIconColor(
          is_muted ? cros_tokens::kCrosSysOnSurfaceVariant
                   : cros_tokens::kCrosSysSystemOnPrimaryContainer);

      break;
    }
    case Style::kRadioActive:
    case Style::kRadioActiveMuted:
    case Style::kRadioInactive: {
      qs_slider->SetSliderStyle(
          active_device_id == device_id_
              ? (is_muted ? Style::kRadioActiveMuted : Style::kRadioActive)
              : Style::kRadioInactive);
      slider_button()->SetVectorIcon(is_muted ? kUnifiedMenuVolumeMuteIcon
                                              : GetVolumeIconForLevel(level));
      slider_button()->SetIconColor(
          active_device_id == device_id_
              ? (is_muted ? cros_tokens::kCrosSysOnSurface
                          : cros_tokens::kCrosSysSystemOnPrimaryContainer)
              : cros_tokens::kCrosSysOnSurfaceVariant);
      break;
    }
    default:
      NOTREACHED();
  }

  // Updates the tooltip for `slider_button()` based on the mute state.
  std::u16string state_tooltip_text = l10n_util::GetStringUTF16(
      is_muted ? IDS_ASH_STATUS_TRAY_VOLUME_STATE_MUTED
               : IDS_ASH_STATUS_TRAY_VOLUME_STATE_ON);
  slider_button()->SetTooltipText(l10n_util::GetStringFUTF16(
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

const gfx::VectorIcon& UnifiedVolumeView::GetVolumeIconForLevel(float level) {
  int index = static_cast<int>(std::ceil(level * kQsVolumeLevels));
  CHECK(index >= 0 && index <= kQsVolumeLevels);
  return *kQsVolumeLevelIcons[index];
}

void UnifiedVolumeView::OnLiveCaptionButtonPressed() {
  Shell::Get()->accessibility_controller()->live_caption().SetEnabled(
      !Shell::Get()->accessibility_controller()->live_caption().enabled());
}

void UnifiedVolumeView::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                  int volume) {
  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnOutputMuteChanged(bool mute_on) {
  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnAudioNodesChanged() {
  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnActiveOutputNodeChanged() {
  // If this view is for the active output node, we need to update the
  // `device_id_` before repaint.
  if (is_active_output_node_) {
    device_id_ = CrasAudioHandler::Get()->GetPrimaryActiveOutputNode();
  }

  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnActiveInputNodeChanged() {
  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnAccessibilityStatusChanged() {
  const bool enabled =
      Shell::Get()->accessibility_controller()->live_caption().enabled();

  // Sets `live_caption_button_` toggle state to update its icon, icon color,
  // and background color.
  live_caption_button_->SetToggled(enabled);

  // Updates the tooltip of `live_caption_button_`.
  std::u16string toggle_tooltip = l10n_util::GetStringUTF16(
      enabled ? IDS_ASH_STATUS_TRAY_LIVE_CAPTION_ENABLED_STATE_TOOLTIP
              : IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP);

  live_caption_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP, toggle_tooltip));
}

void UnifiedVolumeView::ChildVisibilityChanged(views::View* child) {
  DeprecatedLayoutImmediately();
}

void UnifiedVolumeView::VisibilityChanged(View* starting_from,
                                          bool is_visible) {
  Update(/*by_user=*/true);
}

BEGIN_METADATA(UnifiedVolumeView)
END_METADATA

}  // namespace ash
