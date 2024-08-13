// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_MEDIA_CAST_LIST_VIEW_H_
#define ASH_SYSTEM_CAST_MEDIA_CAST_LIST_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class PillButton;

// View ID's.
inline constexpr int kStopCastingButtonId = 100;
inline constexpr int kMediaCastListViewMaxId = kStopCastingButtonId;

// This view displays a list of cast devices that can be clicked on and casted
// to. If it's currently casting, it shows a stop casting button on the header
// entry.
class ASH_EXPORT MediaCastListView
    : public global_media_controls::mojom::DeviceListClient,
      public TrayDetailedView {
  METADATA_HEADER(MediaCastListView, TrayDetailedView)

 public:
  MediaCastListView(
      base::RepeatingClosure stop_casting_callback,
      base::RepeatingCallback<void(const std::string& device_id)>
          start_casting_callback,
      base::RepeatingCallback<void(const bool has_devices)>
          on_devices_updated_callback,
      mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient>
          receiver);

  MediaCastListView(const MediaCastListView&) = delete;
  MediaCastListView& operator=(const MediaCastListView&) = delete;

  ~MediaCastListView() override;

  // global_media_controls::mojom::DeviceListClient:
  void OnDevicesUpdated(
      std::vector<global_media_controls::mojom::DevicePtr> devices) override;
  void OnPermissionRejected() override {}

 private:
  friend class MediaCastAudioSelectorViewTest;
  friend class MediaCastListViewTest;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  // Creates a stop button which, when pressed, stops casting.
  std::unique_ptr<PillButton> CreateStopButton();

  //  Creates the header of the list. If it's currently casting, add the stop
  //  casting button.
  void CreateCastingHeader();

  // Cast item container.
  raw_ptr<views::View> item_container_ = nullptr;

  // Callbacks to stop/start casting.
  base::RepeatingClosure on_stop_casting_callback_;
  base::RepeatingCallback<void(const std::string&)> on_start_casting_callback_;

  // Runs on devices updated.
  base::RepeatingCallback<void(const bool)> on_devices_updated_callback_;

  mojo::Receiver<global_media_controls::mojom::DeviceListClient> receiver_;

  base::WeakPtrFactory<MediaCastListView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_MEDIA_CAST_LIST_VIEW_H_
