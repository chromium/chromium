// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VIDEO_CAPTURE_DEVICE_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VIDEO_CAPTURE_DEVICE_FACTORY_ASH_H_

#include <string>

#include "base/containers/flat_map.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"

namespace crosapi {

class VideoCaptureDeviceAsh;

// This class is the ash-chrome implementation of the VideoCaptureDeviceFactory
// interface. This class must only be used from the main thread.
// It is used as a proxy between Lacros-Chrome and actual
// video_capture::DeviceFactory in Ash-Chrome and also responsible for
// translating structures between crosapi and other components.
// (e.g. gfx, media, video_capture)
class VideoCaptureDeviceFactoryAsh
    : public crosapi::mojom::VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryAsh();
  VideoCaptureDeviceFactoryAsh(const VideoCaptureDeviceFactoryAsh&) = delete;
  VideoCaptureDeviceFactoryAsh& operator=(const VideoCaptureDeviceFactoryAsh&) =
      delete;
  ~VideoCaptureDeviceFactoryAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory>
          receiver);

  // crosapi::mojom::VideoCaptureDeviceFactory:
  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(
      const std::string& device_id,
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDevice> device_receiver,
      CreateDeviceCallback callback) override;

 private:
  // It will be triggered once the connection to the client of
  // video_capture::mojom::Device in Lacros-Chrome is dropped.
  void OnClientConnectionErrorOrClose(const std::string& device_id);

  mojo::Remote<video_capture::mojom::DeviceFactory> device_factory_;

  // The key is the device id used in blink::MediaStreamDevice.
  base::flat_map<std::string, std::unique_ptr<VideoCaptureDeviceAsh>> devices_;

  mojo::ReceiverSet<crosapi::mojom::VideoCaptureDeviceFactory> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VIDEO_CAPTURE_DEVICE_FACTORY_ASH_H_
