// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_FAKE_VIDEO_SOURCE_PROVIDER_H_
#define ASH_CAPTURE_MODE_FAKE_VIDEO_SOURCE_PROVIDER_H_

#include <memory>
#include <string>

#include "ash/capture_mode/fake_camera_device.h"
#include "base/functional/callback_forward.h"
#include "media/base/video_facing.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace ash {

// Defines a fake implementation of the `VideoSourceProvider` mojo interface
// for testing the interaction with the video capture service.
class FakeVideoSourceProvider
    : public video_capture::mojom::VideoSourceProvider {
 public:
  FakeVideoSourceProvider();
  FakeVideoSourceProvider(const FakeVideoSourceProvider&) = delete;
  FakeVideoSourceProvider& operator=(const FakeVideoSourceProvider&) = delete;
  ~FakeVideoSourceProvider() override;

  void set_on_replied_with_source_infos(base::OnceClosure callback) {
    on_replied_with_source_infos_ = std::move(callback);
  }

  void Bind(mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
                pending_receiver);

  // Simulates a fatal error on the camera device whose `device_id` is given.
  void TriggerFatalErrorOnCamera(const std::string& device_id);

  // Simulate connecting and disconnecting a camera device with the given
  // `device_id`, `display_name` and `model_id`.
  void AddFakeCamera(const std::string& device_id,
                     const std::string& display_name,
                     const std::string& model_id,
                     media::VideoFacingMode camera_facing_mode);
  void AddFakeCameraWithoutNotifying(const std::string& device_id,
                                     const std::string& display_name,
                                     const std::string& model_id,
                                     media::VideoFacingMode camera_facing_mode);
  void RemoveFakeCamera(const std::string& device_id);
  void RemoveFakeCameraWithoutNotifying(const std::string& device_id);

  // video_capture::mojom::VideoSourceProvider:
  void GetSourceInfos(GetSourceInfosCallback callback) override;
  void GetVideoSource(
      const std::string& source_id,
      mojo::PendingReceiver<video_capture::mojom::VideoSource> stream) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<video_capture::mojom::Producer> producer,
      mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
          virtual_device_receiver) override {}
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<video_capture::mojom::TextureVirtualDevice>
          virtual_device_receiver) override {}
  void RegisterVirtualDevicesChangedObserver(
      mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver>
          observer,
      bool raise_event_if_virtual_devices_already_present) override {}
  void RegisterDevicesChangedObserver(
      mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver>
          observer) override {}
  void Close(CloseCallback callback) override {}

 private:
  mojo::Receiver<video_capture::mojom::VideoSourceProvider> receiver_{this};

  base::flat_map</*device_id=*/std::string, std::unique_ptr<FakeCameraDevice>>
      devices_map_;

  // A callback that's triggered after this source provider replies back to its
  // client in GetSourceInfos().
  base::OnceClosure on_replied_with_source_infos_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_FAKE_VIDEO_SOURCE_PROVIDER_H_
