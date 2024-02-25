// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAMERA_VIDEO_FRAME_RENDERER_H_
#define ASH_CAPTURE_MODE_CAMERA_VIDEO_FRAME_RENDERER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/capture_mode/camera_video_frame_handler.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class LayerTreeFrameSink;
}  // namespace cc

namespace media {
class VideoResourceUpdater;
}  // namespace media

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace ash {

// Renders the video frames received from the camera video device by creating
// independent compositor frames containing the video frames and submitting them
// on a layer tree frame sink created on the `host_window_`.
class CameraVideoFrameRenderer
    : public capture_mode::CameraVideoFrameHandler::Delegate,
      public viz::BeginFrameObserverBase,
      public cc::LayerTreeFrameSinkClient {
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

  // Initializes this renderer by creating the `layer_tree_frame_sink_` and
  // starts receiving video frames from the camera device.
  void Initialize();

  // CameraVideoFrameHandler::Delegate:
  void OnCameraVideoFrame(scoped_refptr<media::VideoFrame> frame) override;
  void OnFatalErrorOrDisconnection() override;

  // viz::BeginFrameObserverBase:
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) override;

  // cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  std::optional<viz::HitTestRegionList> BuildHitTestData() override;
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void SetTreeActivationCallback(base::RepeatingClosure callback) override;
  void DidReceiveCompositorFrameAck() override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override;
  void DidLoseLayerTreeFrameSink() override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw,
              bool skip_draw) override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;

 private:
  friend class CaptureModeTestApi;

  // Creates and returns a compositor frame for the given `video_frame`.
  viz::CompositorFrame CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      scoped_refptr<media::VideoFrame> video_frame);

  // The window hosting the rendered video frames.
  aura::Window host_window_;

  // The handler that subscribes to the camera video device.
  capture_mode::CameraVideoFrameHandler video_frame_handler_;

  // Used to identify gpu or software resources obtained from a video frame in
  // order for these resources to be given to the Viz display compositor.
  viz::ClientResourceProvider client_resource_provider_;

  // Generates a frame token for the next compositor frame we create.
  viz::FrameTokenGenerator compositor_frame_token_generator_;

  // If not empty, the video frame that will be rendered next when
  // `OnBeginFrameDerivedImpl()` is called.
  scoped_refptr<media::VideoFrame> current_video_frame_;

  // The pixel size and the DSF of the most recent submitted compositor frame.
  // If either changes, we'll need to allocate a new local surface ID.
  gfx::Size last_compositor_frame_size_pixels_;
  float last_compositor_frame_dsf_ = 1.0f;

  scoped_refptr<viz::RasterContextProvider> context_provider_;

  // The layer tree frame sink created from `host_window_`, which is used to
  // submit compositor frames for the camera video frames.
  std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink_;

  // Used to produce the camera video frame's content as resources consumable by
  // the Viz display compositor.
  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater_;

  // The currently observed `BeginFrameSource` which will notify us with
  // `OnBeginFrameDerivedImpl()`.
  raw_ptr<viz::BeginFrameSource> begin_frame_source_ = nullptr;

  // A callback used for tests to be called after `frame` has been rendered.
  base::OnceCallback<void(scoped_refptr<media::VideoFrame> frame)>
      on_video_frame_rendered_for_test_;

  // True if we submitted a compositor frame and are waiting for a call to
  // `DidReceiveCompositorFrameAck()`.
  bool pending_compositor_frame_ack_ = false;

  // Whether the camera video frames should be flipped horizontally around the Y
  // axis so that the camera behaves like a mirror. This can be false for world-
  // facing cameras.
  bool should_flip_frames_horizontally_ = true;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAMERA_VIDEO_FRAME_RENDERER_H_
