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

  gfx::Size GetCaptureSize() const override { return initial_video_size_; }

 private:
  const gfx::Size initial_video_size_;
  const gfx::Size max_video_size_;
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
    capturer->SetAutoThrottlingEnabled(false);
  }

  gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect) const override {
    return crop_region_;
  }

  gfx::Size GetCaptureSize() const override { return crop_region_.size(); }

 private:
  const gfx::Size full_capture_size_;
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

VideoCaptureParams::VideoCaptureParams(viz::FrameSinkId frame_sink_id,
                                       viz::SubtreeCaptureId subtree_capture_id)
    : frame_sink_id_(frame_sink_id), subtree_capture_id_(subtree_capture_id) {
  DCHECK(frame_sink_id_.is_valid());
}

}  // namespace recording
