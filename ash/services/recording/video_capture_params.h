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
#include "ui/gfx/geometry/size.h"

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
  // resolution equal to the given |frame_sink_size| in DIPs. |frame_sink_id|
  // must be valid.
  static std::unique_ptr<VideoCaptureParams> CreateForFullscreenCapture(
      viz::FrameSinkId frame_sink_id,
      const gfx::Size& frame_sink_size);

  // Returns a capture params instance for a recording of a window. The given
  // |frame_sink_id| is either of that window (if it submits compositor frames
  // independently), or of the root window it descends from (if it doesn't
  // submit its compositor frames). In the latter case, the window must be
  // identifiable by a valid |subtree_capture_id| (created by calling
  // aura::window::MakeWindowCapturable() before recording starts).
  // |window_size| is the initial size of the recorded window, and
  // |frame_sink_size| is the current size of the frame sink.
  // |frame_sink_id| must be valid.
  static std::unique_ptr<VideoCaptureParams> CreateForWindowCapture(
      viz::FrameSinkId frame_sink_id,
      viz::SubtreeCaptureId subtree_capture_id,
      const gfx::Size& window_size,
      const gfx::Size& frame_sink_size);

  // Returns a capture params instance for a recording of a partial region of a
  // root window which has the given |frame_sink_id|. The video will be captured
  // at a resolution equal to the given |frame_sink_size| in DIPs, but the
  // resulting video frames will be cropped to the given |crop_region| in DIPs.
  // |frame_sink_id| must be valid.
  static std::unique_ptr<VideoCaptureParams> CreateForRegionCapture(
      viz::FrameSinkId frame_sink_id,
      const gfx::Size& frame_sink_size,
      const gfx::Rect& crop_region);

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }
  gfx::Size current_frame_sink_size() const { return current_frame_sink_size_; }

  // Initializes the given |capturer| (passed by ref) according to the capture
  // parameters. The given |capturer| must be bound before calling this.
  void InitializeVideoCapturer(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) const;

  // Sets the desired resolution constraints on the given |capturer|. By default
  // the size of the recorded frame sink is used. Sub classes can override this
  // behavior if needed.
  virtual void SetCapturerResolutionConstraints(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) const;

  // Returns the bounds to which a video frame, whose
  // |original_frame_visible_rect| is given, should be cropped. If no cropping
  // is desired, |original_frame_visible_rect| is returned. All bounds are in
  // DIPs.
  virtual gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect) const;

  // Returns the size in DIPs with which the video encoder will be initialized.
  virtual gfx::Size GetVideoSize() const = 0;

  // Called when a window, being recorded by the given |capturer|, is moved to
  // a different display whose root window has the given |new_frame_sink_id|,
  // and |new_frame_sink_size| which matches the new display's size.
  // The default implementation is to *crash* the service, as this is only valid
  // when recording a window.
  // Returns true if the video encoder needs to be reconfigured, which happens
  // when the bounds of the new display is different than that of the old
  // display. Returns false otherwise.
  virtual bool OnRecordedWindowChangingRoot(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      viz::FrameSinkId new_frame_sink_id,
      const gfx::Size& new_frame_sink_size) WARN_UNUSED_RESULT;

  // Called when a window being recorded by the given |capturer| is resized
  // (e.g. due to snapping, maximizing, user resizing, ... etc.) to
  // |new_window_size| in DIPs.
  // The default implementation is to *crash* the service, as this is only valid
  // when recording a window.
  // Returns true if the video encoder needs to be reconfigured, indicating that
  // |new_window_size| will result in a change in the video size. False
  // otherwise.
  virtual bool OnRecordedWindowSizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_window_size) WARN_UNUSED_RESULT;

  // Called when the dimensions of the frame sink being recorded is changed to
  // |new_frame_sink_size| in DIPs, which will be used to update the resolution
  // constraints on the given |capturer|.
  // The default implementation updates the resolutions constraints requested
  // from the capturer, so that capture happens at the new frame sink size.
  // Implementations can override this by doing nothing, in this case the new
  // video frames will letterbox to adhere to the initially requested resolution
  // constraints.
  // Returns true if the video encoder needs to be reconfigured, indicating an
  // actual change in the video size. False otherwise.
  virtual bool OnFrameSinkSizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_frame_sink_size) WARN_UNUSED_RESULT;

 protected:
  VideoCaptureParams(viz::FrameSinkId frame_sink_id,
                     viz::SubtreeCaptureId subtree_capture_id,
                     const gfx::Size& current_frame_sink_size);

  viz::FrameSinkId frame_sink_id_;
  const viz::SubtreeCaptureId subtree_capture_id_;
  gfx::Size current_frame_sink_size_;
};

}  // namespace recording

#endif  // ASH_SERVICES_RECORDING_VIDEO_CAPTURE_PARAMS_H_
