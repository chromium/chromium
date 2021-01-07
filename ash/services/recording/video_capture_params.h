// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_RECORDING_VIDEO_CAPTURE_PARAMS_H_
#define ASH_SERVICES_RECORDING_VIDEO_CAPTURE_PARAMS_H_

#include <memory>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom-forward.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace recording {

// Encapsulates the parameters for an ongoing video capture, and knows how to
// initialize a video capturer according to the requested capture source
// (fullscreen, window, or region).
class VideoCaptureParams {
 public:
  VideoCaptureParams(const VideoCaptureParams&) = delete;
  VideoCaptureParams& operator=(const VideoCaptureParams&) = delete;
  virtual ~VideoCaptureParams() = default;

  // Returns a capture params instance for a fullscreen recording of a root
  // window which has the given |frame_sink_id|. The resulting video will have a
  // resolution equal to the given |video_size| in DIPs. |frame_sink_id| must be
  // valid.
  static std::unique_ptr<VideoCaptureParams> CreateForFullscreenCapture(
      viz::FrameSinkId frame_sink_id,
      const gfx::Size& video_size);

  // Returns a capture params instance for a recording of a window. The given
  //|frame_sink_id| is either of that window (if it submits compositor frames
  // independently), or of the root window it descends from (if it doesn't
  // submit its compositor frames). In the latter case, the window must be
  // identifiable by a valid |subtree_capture_id| (created by calling
  // aura::window::MakeWindowCapturable() before recording starts).
  // |initial_video_size| and |max_video_size| specify a range of acceptable
  // capture resolutions in DIPs. The resolution of the output will adapt
  // dynamically as the window being recorded gets resized by the end user (e.g.
  // resized, maximized, fullscreened, ... etc.). |frame_sink_id| must be valid.
  static std::unique_ptr<VideoCaptureParams> CreateForWindowCapture(
      viz::FrameSinkId frame_sink_id,
      viz::SubtreeCaptureId subtree_capture_id,
      const gfx::Size& initial_video_size,
      const gfx::Size& max_video_size);

  // Returns a capture params instance for a recording of a partial region of a
  // root window which has the given |frame_sink_id|.The video will be captured
  // at a resolution equal to the given |full_capture_size| in DIPs, but the
  // resulting video frames will be cropped to the given |crop_region| in DIPs.
  // |frame_sink_id| must be valid.
  static std::unique_ptr<VideoCaptureParams> CreateForRegionCapture(
      viz::FrameSinkId frame_sink_id,
      const gfx::Size& full_capture_size,
      const gfx::Rect& crop_region);

  // Initializes the given |capturer| (passed by ref) according to the capture
  // source (fullscreen, window, or region). The given |capturer| must be bound
  // before calling this.
  virtual void InitializeVideoCapturer(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) const;

  // Returns the bounds to which a video frame, whose
  // |original_frame_visible_rect| is given, should be cropped. If no cropping
  // is desired, |original_frame_visible_rect| is returned. All bounds are in
  // DIPs.
  virtual gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect) const;

  // Returns the size in DIPs with which the audio encoder will be initialized.
  virtual gfx::Size GetCaptureSize() const = 0;

 protected:
  explicit VideoCaptureParams(viz::FrameSinkId frame_sink_id,
                              viz::SubtreeCaptureId subtree_capture_id);

  const viz::FrameSinkId frame_sink_id_;
  const viz::SubtreeCaptureId subtree_capture_id_;
};

}  // namespace recording

#endif  // ASH_SERVICES_RECORDING_VIDEO_CAPTURE_PARAMS_H_
