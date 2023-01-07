// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/video_capture_device_factory_ash.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ash/crosapi/video_capture_device_ash.h"
#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace crosapi {

VideoCaptureDeviceFactoryAsh::VideoCaptureDeviceFactoryAsh() {
  content::GetVideoCaptureService().ConnectToDeviceFactory(
      device_factory_.BindNewPipeAndPassReceiver());
}

VideoCaptureDeviceFactoryAsh::~VideoCaptureDeviceFactoryAsh() = default;

void VideoCaptureDeviceFactoryAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VideoCaptureDeviceFactoryAsh::GetDeviceInfos(
    GetDeviceInfosCallback callback) {
  device_factory_->GetDeviceInfos(std::move(callback));
}

void VideoCaptureDeviceFactoryAsh::CreateDevice(
    const std::string& device_id,
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDevice> device_receiver,
    CreateDeviceCallback callback) {
  mojo::PendingRemote<video_capture::mojom::Device> proxy_remote;
  auto proxy_receiver = proxy_remote.InitWithNewPipeAndPassReceiver();
  auto device_proxy = std::make_unique<VideoCaptureDeviceAsh>(
      std::move(device_receiver), std::move(proxy_remote),
      base::BindOnce(
          &VideoCaptureDeviceFactoryAsh::OnClientConnectionErrorOrClose,
          base::Unretained(this), device_id));

  auto wrapped_callback = base::BindOnce(
      [](CreateDeviceCallback callback, media::VideoCaptureError error_code) {
        crosapi::mojom::DeviceAccessResultCode crosapi_result_code;
        switch (error_code) {
          case media::VideoCaptureError::
              kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice:
            crosapi_result_code =
                crosapi::mojom::DeviceAccessResultCode::NOT_INITIALIZED;
            break;
          case media::VideoCaptureError::kNone:
            crosapi_result_code =
                crosapi::mojom::DeviceAccessResultCode::SUCCESS;
            break;
          default:
            crosapi_result_code =
                crosapi::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND;
        }
        std::move(callback).Run(crosapi_result_code);
      },
      std::move(callback));
  devices_.emplace(device_id, std::move(device_proxy));
  device_factory_->CreateDevice(device_id, std::move(proxy_receiver),
                                std::move(wrapped_callback));
}

void VideoCaptureDeviceFactoryAsh::OnClientConnectionErrorOrClose(
    const std::string& device_id) {
  devices_.erase(device_id);
}

}  // namespace crosapi
