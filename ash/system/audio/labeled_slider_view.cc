// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/labeled_slider_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/audio/audio_detailed_view_utils.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_slider_view.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr auto kDevicesNameViewPreferredSize = gfx::Size(0, 44);
constexpr auto kDevicesTriViewInsets = gfx::Insets::TLBR(0, 24, 0, 32);
constexpr auto kDevicesTriViewBorder = gfx::Insets::VH(0, 4);
constexpr auto kWideDevicesSliderBorder = gfx::Insets::VH(4, 8);
constexpr auto kWideDevicesTriViewInsets = gfx::Insets::TLBR(0, 8, 0, 16);

// Returns the corresponding device name based on the passed in `device`.
std::u16string GetAudioDeviceName(const AudioDevice& device) {
  switch (device.type) {
    case AudioDeviceType::kBluetooth:
    case AudioDeviceType::kBluetoothNbMic:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_BLUETOOTH_DEVICE,
          base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kFrontMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_FRONT_MIC);
    case AudioDeviceType::kHeadphone:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HEADPHONE);
    case AudioDeviceType::kHdmi:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HDMI_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kInternalMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_MIC);
    case AudioDeviceType::kInternalSpeaker:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_SPEAKER);
    case AudioDeviceType::kMic:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_MIC_JACK_DEVICE);
    case AudioDeviceType::kRearMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_REAR_MIC);
    case AudioDeviceType::kUsb:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_USB_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kAlsaLoopback:
    case AudioDeviceType::kHotword:
    case AudioDeviceType::kKeyboardMic:
    case AudioDeviceType::kLineout:
    case AudioDeviceType::kPostDspLoopback:
    case AudioDeviceType::kPostMixLoopback:
    case AudioDeviceType::kOther:
      return base::UTF8ToUTF16(device.display_name);
  }
}

// Customizes the highlight path so that the device container view's focus ring
// follows the slider's size instead of its own size.
// NOTE: This class requires a valid slider during its life cycle.
class DeviceNameContainerHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  DeviceNameContainerHighlightPathGenerator(QuickSettingsSlider* slider,
                                            bool is_wide_slider)
      : slider_(slider), is_wide_slider_(is_wide_slider) {
    CHECK(slider);
  }
  DeviceNameContainerHighlightPathGenerator(
      const DeviceNameContainerHighlightPathGenerator&) = delete;
  DeviceNameContainerHighlightPathGenerator& operator=(
      const DeviceNameContainerHighlightPathGenerator&) = delete;
  ~DeviceNameContainerHighlightPathGenerator() override = default;

 private:
  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    const gfx::Rect slider_bounds = slider_->GetInactiveRadioSliderRect();
    const gfx::RectF bounds(
        slider_bounds.x() +
            (is_wide_slider_ ? 0 : kRadioSliderViewPadding.left()),
        slider_bounds.y(), slider_bounds.width(), slider_bounds.height());
    const gfx::RoundedCornersF rounded(
        slider_->GetInactiveRadioSliderRoundedCornerRadius());
    return gfx::RRectF(bounds, rounded);
  }

  const raw_ptr<QuickSettingsSlider> slider_;
  const bool is_wide_slider_;
};

}  // namespace

LabeledSliderView::LabeledSliderView(TrayDetailedView* detailed_view,
                                     std::unique_ptr<views::View> slider_view,
                                     const AudioDevice& device,
                                     bool is_wide_slider)
    : is_wide_slider_(is_wide_slider) {
  SetUseDefaultFillLayout(true);

  // TODO (b/319941708): Remove this work-around after this bug is fixed.
  // Adding a layer avoids calling `OrphanLayers()` when removing child views.
  // Note that we set the layer of `unified_slider_view_` beneath the layer of
  // `device_name_view_`. Meanwhile, `device_name_view_` is removed before
  // `unified_slider_view_` due to the view order. As a result, the layer of
  // `unified_slider_view_` will be removed from the parent twice, causing a
  // CHECK error.
  SetPaintToLayer();

  // Creates and formats the slider view.
  unified_slider_view_ = views::AsViewClass<UnifiedSliderView>(
      AddChildView(std::move(slider_view)));
  auto* slider =
      views::AsViewClass<QuickSettingsSlider>(unified_slider_view_->slider());
  if (is_wide_slider_) {
    slider->SetBorder(views::CreateEmptyBorder(kWideDevicesSliderBorder));
  }

  // Creates and formats the device name view.
  device_name_view_ = detailed_view->AddScrollListCheckableItem(
      this, gfx::kNoneIcon, GetAudioDeviceName(device), device.active);
  ConfigureDeviceNameView(device);

  // Puts `unified_slider_view_` beneath `device_name_view_`.
  device_name_view_->AddLayerToRegion(unified_slider_view_->layer(),
                                      views::LayerRegion::kBelow);

  ConfigureFocusBehavior(device.active, slider);
}

LabeledSliderView::~LabeledSliderView() {
  // Remove the focus ring before the slider view that highlight path generator
  // of `device_name_view_` depends on is deleted.
  views::FocusRing::Remove(device_name_view_);
}

void LabeledSliderView::ConfigureDeviceNameView(const AudioDevice& device) {
  device_name_view_->SetPaintToLayer();

  // Set this flag to false to make the assigned color id effective.
  // Otherwise it will use `color_utils::BlendForMinContrast()` to improve
  // label readability over the background.
  device_name_view_->text_label()->SetAutoColorReadabilityEnabled(
      /*enabled=*/false);

  device_name_view_->SetPreferredSize(kDevicesNameViewPreferredSize);
  device_name_view_->tri_view()->SetInsets(
      is_wide_slider_ ? kWideDevicesTriViewInsets : kDevicesTriViewInsets);
  device_name_view_->tri_view()->SetContainerBorder(
      TriView::Container::CENTER,
      views::CreateEmptyBorder(kDevicesTriViewBorder));
  const bool is_muted =
      device.is_input
          ? CrasAudioHandler::Get()->IsInputMutedForDevice(device.id)
          : CrasAudioHandler::Get()->IsOutputMutedForDevice(device.id);
  UpdateDeviceContainerColor(device_name_view_, is_muted, device.active);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                        *device_name_view_->text_label());
}

void LabeledSliderView::ConfigureFocusBehavior(const bool is_active,
                                               QuickSettingsSlider* slider) {
  // If this device is the active one, disables event handling on
  // `device_name_view_` so that `slider` can handle the events.
  if (is_active) {
    device_name_view_->SetFocusBehavior(
        HoverHighlightView::FocusBehavior::NEVER);
    device_name_view_->SetCanProcessEventsWithinSubtree(false);
  } else {
    // Installs the customized focus ring path generator for
    // `device_name_view_`.
    device_name_view_->SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(device_name_view_)
        ->SetPathGenerator(
            std::make_unique<DeviceNameContainerHighlightPathGenerator>(
                slider, is_wide_slider_));
    device_name_view_->SetFocusPainter(nullptr);
    views::FocusRing::Get(device_name_view_)
        ->SetColorId(cros_tokens::kCrosSysPrimary);
  }
}

BEGIN_METADATA(LabeledSliderView)
END_METADATA

}  // namespace ash
