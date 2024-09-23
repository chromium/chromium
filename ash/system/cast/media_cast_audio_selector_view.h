// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_MEDIA_CAST_AUDIO_SELECTOR_VIEW_H_
#define ASH_SYSTEM_CAST_MEDIA_CAST_AUDIO_SELECTOR_VIEW_H_

#include <bitset>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/cast/media_cast_list_view.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace global_media_controls {
class MediaItemUIView;
}  // namespace global_media_controls

namespace views {
class View;
}  // namespace views

namespace ash {

// View ID's.
inline constexpr int kListViewContainerId = kMediaCastListViewMaxId + 1;
inline constexpr int kMediaAudioListViewId = kListViewContainerId + 1;
inline constexpr int kMediaCastListViewId = kMediaAudioListViewId + 1;

// The device selector view on Ash side. It shows an audio list view and a cast
// list view. This view will show under the global `MediaUIView`.
// TODO(b/327507429): Add the audio list view.
class ASH_EXPORT MediaCastAudioSelectorView
    : public global_media_controls::MediaItemUIDeviceSelector {
  METADATA_HEADER(MediaCastAudioSelectorView,
                  global_media_controls::MediaItemUIDeviceSelector)
 public:
  MediaCastAudioSelectorView(
      mojo::PendingRemote<global_media_controls::mojom::DeviceListHost>
          device_list_host,
      mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient>
          receiver,
      base::RepeatingClosure stop_casting_callback,
      bool show_devices);
  MediaCastAudioSelectorView(const MediaCastAudioSelectorView&) = delete;
  MediaCastAudioSelectorView& operator=(const MediaCastAudioSelectorView&) =
      delete;
  ~MediaCastAudioSelectorView() override;

  // global_media_controls::MediaItemUIDeviceSelector:
  void SetMediaItemUIView(
      global_media_controls::MediaItemUIView* view) override;
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override;
  void UpdateCurrentAudioDevice(const std::string& current_device_id) override;
  void ShowDevices() override;
  void HideDevices() override;
  bool IsDeviceSelectorExpanded() override;

 private:
  friend class MediaCastAudioSelectorViewTest;

  // Device types that are in `list_view_container_`.
  enum class DeviceType {
    kCastDevice,
    kAudioDevice,
    kMax = kAudioDevice + 1,
  };

  // The callback that is passed to the `MediaCastListView` to inform the
  // panel that the devices is updated.
  void OnDevicesUpdated(DeviceType device_type, bool has_devices);

  // The callback that is passed to the `MediaCastListView` when a cast item
  // entry is pressed.
  void OnCastDeviceSelected(const std::string& device_id);

  bool is_expanded_ = false;

  // This bitset carries the flags indicating the presence of cast devices and
  // audio devices within the device list.
  std::bitset<static_cast<int>(DeviceType::kMax)> device_type_bits_;

  // The container that carries the audio list view and the cast list view.
  raw_ptr<views::View> list_view_container_ = nullptr;

  // The panel view of the media bubble.
  raw_ptr<global_media_controls::MediaItemUIView> media_item_ui_ = nullptr;

  // Provides access to devices on which the media can be casted.
  mojo::Remote<global_media_controls::mojom::DeviceListHost> device_list_host_;

  base::WeakPtrFactory<MediaCastAudioSelectorView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_MEDIA_CAST_AUDIO_SELECTOR_VIEW_H_
