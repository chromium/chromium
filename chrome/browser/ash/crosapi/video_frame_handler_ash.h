// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VIDEO_FRAME_HANDLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VIDEO_FRAME_HANDLER_ASH_H_

#include <vector>

#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/scoped_access_permission.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace crosapi {

// It is used as a proxy to communicate between actual VideoFrameHandler on
// Lacros-Chrome and the actual video capture device on Ash-Chrome. Since we
// have simplified some structures in crosapi video capture interface to reduce
// dependencies to other components, this class should also be responsible for
// translating those structures between the interfaces.
class VideoFrameHandlerAsh : public video_capture::mojom::VideoFrameHandler {
 public:
  VideoFrameHandlerAsh(
      mojo::PendingReceiver<video_capture::mojom::VideoFrameHandler>
          handler_receiver,
      mojo::PendingRemote<crosapi::mojom::VideoFrameHandler> proxy_remote);
  VideoFrameHandlerAsh(const VideoFrameHandlerAsh&) = delete;
  VideoFrameHandlerAsh& operator=(const VideoFrameHandlerAsh&) = delete;
  ~VideoFrameHandlerAsh() override;

  class AccessPermissionProxy : public crosapi::mojom::ScopedAccessPermission {
   public:
    AccessPermissionProxy(
        mojo::PendingRemote<video_capture::mojom::ScopedAccessPermission>
            remote);
    AccessPermissionProxy(const AccessPermissionProxy&) = delete;
    AccessPermissionProxy& operator=(const AccessPermissionProxy&) = delete;
    ~AccessPermissionProxy() override;

   private:
    mojo::Remote<video_capture::mojom::ScopedAccessPermission> remote_;
  };

 private:
  // video_capture::mojom::VideoFrameHandler implementation.
  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(
      video_capture::mojom::ReadyFrameInBufferPtr buffer,
      std::vector<video_capture::mojom::ReadyFrameInBufferPtr> scaled_buffers)
      override;
  void OnBufferRetired(int buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

  mojo::Receiver<video_capture::mojom::VideoFrameHandler> receiver_{this};

  mojo::Remote<crosapi::mojom::VideoFrameHandler> proxy_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VIDEO_FRAME_HANDLER_ASH_H_
