// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/media_cast_audio_selector_view.h"

#include <utility>

#include "ash/system/audio/output_audio_sliders_view.h"
#include "ash/system/cast/media_cast_list_view.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/vector_icons/vector_icons.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {
// Constants for the `MediaCastAudioSelectorView`

// Using a fixed width for this view which will be CHECKed on the global media
// side (`media_item_ui_view`).
constexpr int kSelectorViewWidth = 400;

}  // namespace

MediaCastAudioSelectorView::MediaCastAudioSelectorView(
    mojo::PendingRemote<global_media_controls::mojom::DeviceListHost>
        device_list_host,
    mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient>
        receiver,
    base::RepeatingClosure stop_casting_callback,
    bool show_devices)
    : device_list_host_(std::move(device_list_host)) {
  views::Builder<views::View>(this)
      .SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT))
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      .SetPreferredSize(
          gfx::Size(kSelectorViewWidth, GetHeightForWidth(kSelectorViewWidth)))
      .AddChildren(
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&list_view_container_)
              .SetID(kListViewContainerId)
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetVisible(false)
              .AddChildren(
                  views::Builder<views::View>(
                      std::make_unique<OutputAudioSlidersView>(
                          base::BindRepeating(
                              &MediaCastAudioSelectorView::OnDevicesUpdated,
                              base::Unretained(this),
                              DeviceType::kAudioDevice)))
                      .SetID(kMediaAudioListViewId))
              .AddChildren(
                  views::Builder<views::View>(
                      std::make_unique<MediaCastListView>(
                          std::move(stop_casting_callback),
                          base::BindRepeating(
                              &MediaCastAudioSelectorView::OnCastDeviceSelected,
                              base::Unretained(this)),
                          base::BindRepeating(
                              &MediaCastAudioSelectorView::OnDevicesUpdated,
                              base::Unretained(this), DeviceType::kCastDevice),
                          std::move(receiver)))
                      .SetID(kMediaCastListViewId)))
      .BuildChildren();

  if (show_devices) {
    ShowDevices();
  }

  DeprecatedLayoutImmediately();
}

MediaCastAudioSelectorView::~MediaCastAudioSelectorView() = default;

void MediaCastAudioSelectorView::SetMediaItemUIView(
    global_media_controls::MediaItemUIView* view) {
  media_item_ui_ = view;
}

// The colors are set with ash color ids, so we don't need to override this
// method.
void MediaCastAudioSelectorView::OnColorsChanged(SkColor foreground_color,
                                                 SkColor background_color) {}

// This audio device related feature (under the flag
// `media::kGlobalMediaControlsSeamlessTransfer`) were never launched, so no
// longer maintained. On ash side here we override it with an empty
// implementation, and we add our own output audio list to this view.
void MediaCastAudioSelectorView::UpdateCurrentAudioDevice(
    const std::string& current_device_id) {}

void MediaCastAudioSelectorView::MediaCastAudioSelectorView::ShowDevices() {
  DCHECK(!is_expanded_);
  is_expanded_ = true;
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);

  list_view_container_->SetVisible(true);

  // Focus the first available device when the device list is shown for
  // accessibility.
  // TODO(b/327507429): Revisit the logic here when the audio list view is
  // added to see which view should be focused on.
  if (const auto& children = list_view_container_->children();
      !children.empty()) {
    children[0]->RequestFocus();
  }
}

void MediaCastAudioSelectorView::HideDevices() {
  DCHECK(is_expanded_);
  is_expanded_ = false;
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);

  list_view_container_->SetVisible(false);
  PreferredSizeChanged();
}

bool MediaCastAudioSelectorView::IsDeviceSelectorExpanded() {
  return is_expanded_;
}

void MediaCastAudioSelectorView::OnDevicesUpdated(DeviceType device_type,
                                                  bool has_devices) {
  device_type_bits_[static_cast<int>(device_type)] = has_devices;

  if (media_item_ui_) {
    media_item_ui_->OnDeviceSelectorViewDevicesChanged(device_type_bits_.any());
  }
}

void MediaCastAudioSelectorView::OnCastDeviceSelected(
    const std::string& device_id) {
  if (device_list_host_) {
    device_list_host_->SelectDevice(device_id);
  }
}

BEGIN_METADATA(MediaCastAudioSelectorView)
END_METADATA

}  // namespace ash
