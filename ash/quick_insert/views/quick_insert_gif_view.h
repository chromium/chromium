// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_GIF_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_GIF_VIEW_H_

#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace ash {

namespace image_util {
struct AnimationFrame;
}  // namespace image_util

class ASH_EXPORT QuickInsertGifView : public views::ImageView {
  METADATA_HEADER(QuickInsertGifView, views::ImageView)

 public:
  using FramesFetchedCallback =
      base::OnceCallback<void(std::vector<image_util::AnimationFrame>)>;
  using FramesFetcher =
      base::OnceCallback<std::unique_ptr<network::SimpleURLLoader>(
          FramesFetchedCallback)>;

  using PreviewImageFetchedCallback =
      base::OnceCallback<void(const gfx::ImageSkia&)>;
  using PreviewImageFetcher =
      base::OnceCallback<std::unique_ptr<network::SimpleURLLoader>(
          PreviewImageFetchedCallback)>;

  QuickInsertGifView(FramesFetcher frames_fetcher,
                     PreviewImageFetcher preview_image_fetcher,
                     const gfx::Size& original_dimensions);
  QuickInsertGifView(const QuickInsertGifView&) = delete;
  QuickInsertGifView& operator=(const QuickInsertGifView&) = delete;
  ~QuickInsertGifView() override;

  // views::ImageViewBase:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateFrame();
  void OnFramesFetched(std::vector<image_util::AnimationFrame> frames);

  void OnPreviewImageFetched(const gfx::ImageSkia& preview_image);

  void RecordFetchFramesTime();

  // Original dimensions of the gif, used to preserve aspect ratio when
  // resizing.
  gfx::Size original_dimensions_;

  std::unique_ptr<network::SimpleURLLoader> preview_request_;
  std::unique_ptr<network::SimpleURLLoader> frames_request_;

  // The decoded gif frames.
  std::vector<image_util::AnimationFrame> frames_;

  // Timer to call `UpdateFrame` when the next frame should be shown.
  base::OneShotTimer update_frame_timer_;

  // Index of the frame to show on the next call to `UpdateFrame`.
  size_t next_frame_index_ = 0;

  std::optional<base::TimeTicks> fetch_frames_start_time_;

  base::WeakPtrFactory<QuickInsertGifView> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, QuickInsertGifView, views::ImageView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::QuickInsertGifView)

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_GIF_VIEW_H_
