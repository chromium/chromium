// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/fake_video_source_provider.h"

#include "base/check.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/ranges/algorithm.h"
#include "base/system/system_monitor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/capture/video/video_capture_device_descriptor.h"

namespace ash {

namespace {

// Defines a predicate that when invoked on a `VideoCaptureDeviceInfo` instance
// returns true if it has the same given `device_id`.
struct HasSameDeviceId {
  explicit HasSameDeviceId(const std::string& device_id)
      : device_id(device_id) {}

  bool operator()(const media::VideoCaptureDeviceInfo& device) const {
    return device.descriptor.device_id == device_id;
  }

 private:
  const std::string& device_id;
};

// Triggers a notification that video capture devices have changed.
void NotifyVideoCaptureDevicesChanged() {
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
}

}  // namespace

FakeVideoSourceProvider::FakeVideoSourceProvider() = default;

FakeVideoSourceProvider::~FakeVideoSourceProvider() = default;

void FakeVideoSourceProvider::Bind(
    mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void FakeVideoSourceProvider::AddFakeCamera(const std::string& device_id,
                                            const std::string& display_name,
                                            const std::string& model_id) {
  DCHECK(base::ranges::none_of(devices_, HasSameDeviceId(device_id)));
  devices_.emplace_back(media::VideoCaptureDeviceDescriptor(
      display_name, device_id, model_id, media::VideoCaptureApi::UNKNOWN,
      media::VideoCaptureControlSupport()));
  NotifyVideoCaptureDevicesChanged();
}

void FakeVideoSourceProvider::RemoveFakeCamera(const std::string& device_id) {
  base::EraseIf(devices_, HasSameDeviceId(device_id));
  NotifyVideoCaptureDevicesChanged();
}

void FakeVideoSourceProvider::GetSourceInfos(GetSourceInfosCallback callback) {
  DCHECK(callback);

  // Simulate the asynchronously behavior of the actual VideoSourceProvider
  // which does a lot of asynchronous and mojo calls.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), devices_));

  if (on_replied_with_source_infos_)
    std::move(on_replied_with_source_infos_).Run();
}

}  // namespace ash
