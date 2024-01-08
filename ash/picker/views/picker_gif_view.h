// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_GIF_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_GIF_VIEW_H_

#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ash {

namespace image_util {
struct AnimationFrame;
}  // namespace image_util

class ASH_EXPORT PickerGifView : public views::ImageView {
  METADATA_HEADER(PickerGifView, views::View)

 public:
  using FramesFetchedCallback =
      base::OnceCallback<void(std::vector<image_util::AnimationFrame>)>;
  using FramesFetcher = base::OnceCallback<void(FramesFetchedCallback)>;

  PickerGifView(FramesFetcher frames_fetcher, const gfx::Size& image_size);
  PickerGifView(const PickerGifView&) = delete;
  PickerGifView& operator=(const PickerGifView&) = delete;
  ~PickerGifView() override;

  // views::ImageViewBase:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateFrame();
  void OnFramesFetched(std::vector<image_util::AnimationFrame> frames);

  gfx::Size image_size_;

  // The decoded gif frames.
  std::vector<image_util::AnimationFrame> frames_;

  // Timer to call `UpdateFrame` when the next frame should be shown.
  base::OneShotTimer update_frame_timer_;

  // Index of the frame to show on the next call to `UpdateFrame`.
  size_t next_frame_index_ = 0;

  base::WeakPtrFactory<PickerGifView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_GIF_VIEW_H_
