// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SCREEN_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SCREEN_MANAGER_ASH_H_

#include <stdint.h>

#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/image/image.h"

namespace content {
struct DesktopMediaID;
}

namespace crosapi {

// This class is the ash-chrome implementation of the ScreenManager interface.
// This class must only be used from the main thread.
class ScreenManagerAsh : public mojom::ScreenManager {
 public:
  ScreenManagerAsh();
  ScreenManagerAsh(const ScreenManagerAsh&) = delete;
  ScreenManagerAsh& operator=(const ScreenManagerAsh&) = delete;
  ~ScreenManagerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ScreenManager> receiver);

  // crosapi::mojom::ScreenManager:
  void DeprecatedTakeScreenSnapshot(
      DeprecatedTakeScreenSnapshotCallback callback) override;
  void DeprecatedListWindows(DeprecatedListWindowsCallback callback) override;
  void DeprecatedTakeWindowSnapshot(
      uint64_t id,
      DeprecatedTakeWindowSnapshotCallback callback) override;
  void GetScreenCapturer(
      mojo::PendingReceiver<mojom::SnapshotCapturer> receiver) override;
  void GetWindowCapturer(
      mojo::PendingReceiver<mojom::SnapshotCapturer> receiver) override;
  void GetScreenVideoCapturer(
      mojo::PendingReceiver<mojom::VideoCaptureDevice> receiver,
      uint64_t screen_id) override;
  void GetWindowVideoCapturer(
      mojo::PendingReceiver<mojom::VideoCaptureDevice> receiver,
      uint64_t window_id) override;

  // Returns window by ID if present.
  aura::Window* GetWindowById(uint64_t id) const;

 private:
  class ScreenCapturerImpl;
  class WindowCapturerImpl;

  ScreenCapturerImpl* GetScreenCapturerImpl();
  WindowCapturerImpl* GetWindowCapturerImpl();
  void CreateVideoCaptureDevice(
      mojo::PendingReceiver<mojom::VideoCaptureDevice> receiver,
      const content::DesktopMediaID& device_id);

  std::unique_ptr<ScreenCapturerImpl> screen_capturer_impl_;
  std::unique_ptr<WindowCapturerImpl> window_capturer_impl_;

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes. This is needed by
  // WebRTC.
  mojo::ReceiverSet<mojom::ScreenManager> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SCREEN_MANAGER_ASH_H_
