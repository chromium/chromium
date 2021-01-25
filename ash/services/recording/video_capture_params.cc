// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/recording/video_capture_params.h"

#include "ash/services/recording/recording_service_constants.h"
#include "base/check.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "media/base/video_types.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace recording {

namespace {

// -----------------------------------------------------------------------------
// FullscreenCaptureParams:

class FullscreenCaptureParams : public VideoCaptureParams {
 public:
  FullscreenCaptureParams(viz::FrameSinkId frame_sink_id,
                          const gfx::Size& video_size)
      : VideoCaptureParams(frame_sink_id, viz::SubtreeCaptureId()),
        video_size_(video_size) {}
  FullscreenCaptureParams(const FullscreenCaptureParams&) = delete;
  FullscreenCaptureParams& operator=(const FullscreenCaptureParams&) = delete;
  ~FullscreenCaptureParams() override = default;

  // VideoCaptureParams:
  void InitializeVideoCapturer(mojo::Remote<viz::mojom::FrameSinkVideoCapturer>&
                                   capturer) const override {
    VideoCaptureParams::InitializeVideoCapturer(capturer);
    capturer->SetResolutionConstraints(video_size_, video_size_,
                                       /*use_fixed_aspect_ratio=*/true);
    capturer->SetAutoThrottlingEnabled(false);
  }

  gfx::Size GetCaptureSize() const override { return video_size_; }

 private:
  const gfx::Size video_size_;
};

// -----------------------------------------------------------------------------
// WindowCaptureParams:

class WindowCaptureParams : public VideoCaptureParams {
 public:
  WindowCaptureParams(viz::FrameSinkId frame_sink_id,
                      viz::SubtreeCaptureId subtree_capture_id,
                      const gfx::Size& initial_video_size,
                      const gfx::Size& max_video_size)
      : VideoCaptureParams(frame_sink_id, subtree_capture_id),
        initial_video_size_(initial_video_size),
        max_video_size_(max_video_size) {}
  WindowCaptureParams(const WindowCaptureParams&) = delete;
  WindowCaptureParams& operator=(const WindowCaptureParams&) = delete;
  ~WindowCaptureParams() override = default;

  // VideoCaptureParams:
  void InitializeVideoCapturer(mojo::Remote<viz::mojom::FrameSinkVideoCapturer>&
                                   capturer) const override {
    VideoCaptureParams::InitializeVideoCapturer(capturer);
    capturer->SetResolutionConstraints(initial_video_size_, max_video_size_,
                                       /*use_fixed_aspect_ratio=*/false);
    capturer->SetAutoThrottlingEnabled(true);
  }

  gfx::Size GetCaptureSize() const override {
    // For now, the capturer sends us video frames whose sizes are equal to the
    // size of the root on which the window resides. Therefore,
    // |max_video_size_| should be used to initialize the video encoder.
    // Otherwise, the pixels of the output video will be squished. With this
    // approach, it's possible to resize the window within those bounds without
    // having to change the size of the output video. However, this may not be
    // a desired way.
    // TODO(https://crbug.com/1165708): Investigate how to fix this in the
    // capturer for M-89 or M-90.
    return max_video_size_;
  }

  bool OnRecordedWindowChangingRoot(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      viz::FrameSinkId new_frame_sink_id,
      const gfx::Size& new_max_video_size) override {
    DCHECK(new_frame_sink_id.is_valid());

    // The video encoder deals with video frames. Changing the frame sink ID
    // doesn't affect the encoder. What affects it is a change in the video
    // frames size.
    const bool should_reconfigure_video_encoder =
        max_video_size_ != new_max_video_size;

    max_video_size_ = new_max_video_size;
    frame_sink_id_ = new_frame_sink_id;
    capturer->SetResolutionConstraints(initial_video_size_, max_video_size_,
                                       /*use_fixed_aspect_ratio=*/false);
    capturer->ChangeTarget(frame_sink_id_, subtree_capture_id_);

    return should_reconfigure_video_encoder;
  }

  bool OnDisplaySizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_display_size) override {
    if (new_display_size == max_video_size_)
      return false;

    max_video_size_ = new_display_size;
    capturer->SetResolutionConstraints(initial_video_size_, max_video_size_,
                                       /*use_fixed_aspect_ratio=*/false);
    return true;
  }

 private:
  const gfx::Size initial_video_size_;
  gfx::Size max_video_size_;
};

// -----------------------------------------------------------------------------
// RegionCaptureParams:

class RegionCaptureParams : public VideoCaptureParams {
 public:
  RegionCaptureParams(viz::FrameSinkId frame_sink_id,
                      const gfx::Size& full_capture_size,
                      const gfx::Rect& crop_region)
      : VideoCaptureParams(frame_sink_id, viz::SubtreeCaptureId()),
        full_capture_size_(full_capture_size),
        crop_region_(crop_region) {}
  RegionCaptureParams(const RegionCaptureParams&) = delete;
  RegionCaptureParams& operator=(const RegionCaptureParams&) = delete;
  ~RegionCaptureParams() override = default;

  // VideoCaptureParams:
  void InitializeVideoCapturer(mojo::Remote<viz::mojom::FrameSinkVideoCapturer>&
                                   capturer) const override {
    VideoCaptureParams::InitializeVideoCapturer(capturer);
    capturer->SetResolutionConstraints(full_capture_size_, full_capture_size_,
                                       /*use_fixed_aspect_ratio=*/true);
    capturer->SetAutoThrottlingEnabled(true);
  }

  gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect) const override {
    // We can't crop the video frame by an invalid bounds. The crop bounds must
    // be contained within the original frame bounds.
    gfx::Rect visible_rect = original_frame_visible_rect;
    visible_rect.Intersect(crop_region_);
    return visible_rect;
  }

  gfx::Size GetCaptureSize() const override {
    return GetVideoFrameVisibleRect(gfx::Rect(full_capture_size_)).size();
  }

  bool OnDisplaySizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_display_size) override {
    if (new_display_size == full_capture_size_)
      return false;

    full_capture_size_ = new_display_size;
    capturer->SetResolutionConstraints(full_capture_size_, full_capture_size_,
                                       /*use_fixed_aspect_ratio=*/true);
    return true;
  }

 private:
  gfx::Size full_capture_size_;
  const gfx::Rect crop_region_;
};

}  // namespace

// -----------------------------------------------------------------------------
// VideoCaptureParams:

// static
std::unique_ptr<VideoCaptureParams>
VideoCaptureParams::CreateForFullscreenCapture(viz::FrameSinkId frame_sink_id,
                                               const gfx::Size& video_size) {
  return std::make_unique<FullscreenCaptureParams>(frame_sink_id, video_size);
}

// static
std::unique_ptr<VideoCaptureParams> VideoCaptureParams::CreateForWindowCapture(
    viz::FrameSinkId frame_sink_id,
    viz::SubtreeCaptureId subtree_capture_id,
    const gfx::Size& initial_video_size,
    const gfx::Size& max_video_size) {
  return std::make_unique<WindowCaptureParams>(
      frame_sink_id, subtree_capture_id, initial_video_size, max_video_size);
}

// static
std::unique_ptr<VideoCaptureParams> VideoCaptureParams::CreateForRegionCapture(
    viz::FrameSinkId frame_sink_id,
    const gfx::Size& full_capture_size,
    const gfx::Rect& crop_region) {
  return std::make_unique<RegionCaptureParams>(frame_sink_id, full_capture_size,
                                               crop_region);
}

void VideoCaptureParams::InitializeVideoCapturer(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) const {
  DCHECK(capturer);

  capturer->SetMinCapturePeriod(kMinCapturePeriod);
  capturer->SetMinSizeChangePeriod(kMinPeriodForResizeThrottling);
  // TODO(afakhry): Discuss with //media/ team the implications of color space
  // conversions.
  capturer->SetFormat(media::PIXEL_FORMAT_I420, kColorSpace);
  capturer->ChangeTarget(frame_sink_id_, subtree_capture_id_);
}

gfx::Rect VideoCaptureParams::GetVideoFrameVisibleRect(
    const gfx::Rect& original_frame_visible_rect) const {
  return original_frame_visible_rect;
}

bool VideoCaptureParams::OnRecordedWindowChangingRoot(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    viz::FrameSinkId new_frame_sink_id,
    const gfx::Size& new_max_video_size) {
  CHECK(false) << "This can only be called when recording a window";
  return false;
}

bool VideoCaptureParams::OnDisplaySizeChanged(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    const gfx::Size& new_display_size) {
  CHECK(false)
      << "This can only be called when recording a window or a partial region";
  return false;
}

VideoCaptureParams::VideoCaptureParams(viz::FrameSinkId frame_sink_id,
                                       viz::SubtreeCaptureId subtree_capture_id)
    : frame_sink_id_(frame_sink_id), subtree_capture_id_(subtree_capture_id) {
  DCHECK(frame_sink_id_.is_valid());
}

}  // namespace recording
