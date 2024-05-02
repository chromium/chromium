// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/output_audio_sliders_view.h"

#include <iterator>
#include <memory>
#include <optional>
#include <utility>

#include "ash/system/audio/audio_detailed_view_utils.h"
#include "ash/system/audio/labeled_slider_view.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/audio/unified_volume_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {
constexpr auto kSliderContainerPadding = gfx::Insets::TLBR(20, 0, 0, 0);
}

OutputAudioSlidersView::OutputAudioSlidersView(
    base::RepeatingCallback<void(/*has_devices=*/bool)>
        on_devices_updated_callback)
    : TrayDetailedView(/*delegate=*/nullptr),
      on_devices_updated_callback_(std::move(on_devices_updated_callback)) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  // Creates and adds the slider container.
  AddChildView(views::Builder<views::BoxLayoutView>()
                   .CopyAddressTo(&slider_container_)
                   .SetBorder(views::CreateEmptyBorder(kSliderContainerPadding))
                   .SetOrientation(views::BoxLayout::Orientation::kVertical)
                   .Build());

  Update();
}

OutputAudioSlidersView::~OutputAudioSlidersView() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void OutputAudioSlidersView::HandleViewClicked(views::View* view) {
  AudioDeviceViewMap::iterator iter = output_devices_by_name_views_.find(view);
  if (iter == output_devices_by_name_views_.end()) {
    return;
  }

  // If the clicked view is focused, save the id of this device to preserve the
  // focus ring.
  const AudioDevice& device = iter->second;
  if (view->HasFocus()) {
    focused_device_id_ = device.id;
  }

  CrasAudioHandler::Get()->SwitchToDevice(device, /*notify=*/true,
                                          DeviceActivateType::kActivateByUser);
}

void OutputAudioSlidersView::OnActiveOutputNodeChanged() {
  Update();
}

void OutputAudioSlidersView::OnAudioNodesChanged() {
  Update();
}

void OutputAudioSlidersView::OnOutputMuteChanged(bool mute_on) {
  MaybeUpdateActiveDeviceColor(/*is_input=*/false, mute_on,
                               output_devices_by_name_views_);
}

void OutputAudioSlidersView::Update() {
  AudioDeviceList all_devices;
  CrasAudioHandler::Get()->GetAudioDevices(&all_devices);

  // Only display devices if they are for simple usage and are output devices.
  AudioDeviceList output_devices;
  base::ranges::copy_if(
      all_devices, std::back_inserter(output_devices), [](const auto& device) {
        return device.is_for_simple_usage() && !device.is_input;
      });

  output_devices_by_name_views_.clear();
  slider_container_->RemoveAllChildViews();

  // Inform the Media Panel view on devices update.
  on_devices_updated_callback_.Run(
      /*has_devices=*/!output_devices.empty());

  if (output_devices.empty()) {
    return;
  }

  // Adds audio output devices.
  for (const auto& device : output_devices) {
    auto* labeled_slider_view = views::AsViewClass<LabeledSliderView>(
        slider_container_->AddChildView(std::make_unique<LabeledSliderView>(
            /*detailed_view=*/this,
            unified_volume_slider_controller_.CreateVolumeSlider(
                device.id, /*inside_padding=*/gfx::Insets()),
            device,
            /*is_wide_slider=*/true)));
    output_devices_by_name_views_[labeled_slider_view->device_name_view()] =
        device;

    // If the `labeled_slider_view` of this device is previously focused and
    // then becomes active, the slider of this device should preserve the focus.
    if (focused_device_id_ == device.id && device.active) {
      labeled_slider_view->unified_slider_view()->slider()->RequestFocus();
      focused_device_id_ = std::nullopt;
    }
  }
}

BEGIN_METADATA(OutputAudioSlidersView)
END_METADATA

}  // namespace ash
