// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAMERA_VIDEO_FRAME_RENDERER_H_
#define ASH_CAPTURE_MODE_CAMERA_VIDEO_FRAME_RENDERER_H_

#include <memory>

#include "ash/frame_sink/frame_sink_host.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/capture_mode/camera_video_frame_handler.h"
#include "ui/aura/window.h"

namespace cc {
class ResourcePool;
}  // namespace cc

namespace media {
class VideoFrame;
class VideoResourceUpdater;
}  // namespace media

namespace viz {
class ClientResourceProvider;
class RasterContextProvider;
}  // namespace viz

namespace ash {

// Renders the video frames received from the camera video device by creating
// independent compositor frames containing the video frames and submitting them
// on a layer tree frame sink created on the `host_window_`.
class CameraVideoFrameRenderer
    : public FrameSinkHost,
      public capture_mode::CameraVideoFrameHandler::Delegate {
 public:
  CameraVideoFrameRenderer(
      mojo::Remote<video_capture::mojom::VideoSource> camera_video_source,
      const media::VideoCaptureFormat& capture_format,
      bool should_flip_frames_horizontally);
  CameraVideoFrameRenderer(const CameraVideoFrameRenderer&) = delete;
  CameraVideoFrameRenderer& operator=(const CameraVideoFrameRenderer&) = delete;
  ~CameraVideoFrameRenderer() override;

  aura::Window* host_window() { return &host_window_; }
  bool should_flip_frames_horizontally() const {
    return should_flip_frames_horizontally_;
  }

  // FrameSinkHost:
  void Init(aura::Window* host_window) override;

  // CameraVideoFrameHandler::Delegate:
  void OnCameraVideoFrame(scoped_refptr<media::VideoFrame> frame) override;
  void OnFatalErrorOrDisconnection() override;

 protected:
  // FrameSinkHost:
  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      viz::ClientResourceProvider& client_resource_provider,
      cc::ResourcePool& resource_pool,
      bool auto_update) override;
  void OnFrameSinkLost() override;

 private:
  friend class CaptureModeTestApi;

  void MaybeCreateVideoResourceUpdater(
      viz::ClientResourceProvider& client_resource_provider);

  // The window hosting the rendered video frames.
  aura::Window host_window_;

  // The handler that subscribes to the camera video device.
  capture_mode::CameraVideoFrameHandler video_frame_handler_;

  // If not empty, the video frame that will be rendered next when
  // `CreateCompositorFrame()` is called.
  scoped_refptr<media::VideoFrame> current_video_frame_;

  scoped_refptr<viz::RasterContextProvider> context_provider_;

  raw_ptr<viz::ClientResourceProvider> video_resource_provider_ = nullptr;

  // Used to produce the camera video frame's content as resources consumable by
  // the Viz display compositor.
  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater_;

  // A callback used for tests to be called after `frame` has been rendered.
  base::OnceCallback<void(scoped_refptr<media::VideoFrame> frame)>
      on_video_frame_rendered_for_test_;

  // Whether the camera video frames should be flipped horizontally around the Y
  // axis so that the camera behaves like a mirror. This can be false for world-
  // facing cameras.
  bool should_flip_frames_horizontally_ = true;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAMERA_VIDEO_FRAME_RENDERER_H_
