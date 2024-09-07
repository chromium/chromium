// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_gif_view.h"

#include <optional>

#include "ash/public/cpp/image_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

constexpr int kPickerGifCornerRadius = 8;

// We use a duration of 100ms for frames that specify a duration of <= 10ms.
// This is to follow the behavior of blink (see http://webkit.org/b/36082 for
// more information).
constexpr base::TimeDelta kShortFrameDurationThreshold = base::Milliseconds(10);
constexpr base::TimeDelta kAdjustedDurationForShortFrames =
    base::Milliseconds(100);

}  // namespace

PickerGifView::PickerGifView(FramesFetcher frames_fetcher,
                             PreviewImageFetcher preview_image_fetcher,
                             const gfx::Size& original_dimensions)
    : original_dimensions_(original_dimensions) {
  // Show a placeholder rect while the gif loads.
  views::Builder<PickerGifView>(this)
      .SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysAppBaseShaded, kPickerGifCornerRadius))
      .SetImage(ui::ImageModel::FromImageSkia(
          image_util::CreateEmptyImage(original_dimensions)))
      .BuildChildren();

  fetch_frames_start_time_ = base::TimeTicks::Now();
  std::move(preview_image_fetcher)
      .Run(base::BindOnce(&PickerGifView::OnPreviewImageFetched,
                          weak_factory_.GetWeakPtr()));
  std::move(frames_fetcher)
      .Run(base::BindOnce(&PickerGifView::OnFramesFetched,
                          weak_factory_.GetWeakPtr()));
}

PickerGifView::~PickerGifView() = default;

gfx::Size PickerGifView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int width = 0;
  if (!available_size.width().is_bounded()) {
    width = views::ImageView::CalculatePreferredSize(available_size).width();
  } else {
    width = available_size.width().value();
  }

  const int height = original_dimensions_.width() == 0
                         ? 0
                         : (width * original_dimensions_.height()) /
                               original_dimensions_.width();
  return gfx::Size(width, height);
}

void PickerGifView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::ImageView::OnBoundsChanged(previous_bounds);

  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(GetImageBounds()),
                    SkIntToScalar(kPickerGifCornerRadius),
                    SkIntToScalar(kPickerGifCornerRadius));
  SetClipPath(path);
}

void PickerGifView::UpdateFrame() {
  CHECK(next_frame_index_ < frames_.size());
  SetImage(ui::ImageModel::FromImageSkia(frames_[next_frame_index_].image));

  // Schedule next frame update.
  update_frame_timer_.Start(
      FROM_HERE, frames_[next_frame_index_].duration,
      base::BindOnce(&PickerGifView::UpdateFrame, weak_factory_.GetWeakPtr()));
  next_frame_index_ = (next_frame_index_ + 1) % frames_.size();
}

void PickerGifView::OnFramesFetched(
    std::vector<image_util::AnimationFrame> frames) {
  if (frames.empty()) {
    // TODO: b/316936723 - Handle frames being empty.
    return;
  }

  frames_.reserve(frames.size());
  for (auto& frame : frames) {
    if (frame.duration <= kShortFrameDurationThreshold) {
      frame.duration = kAdjustedDurationForShortFrames;
    }
    frames_.push_back(std::move(frame));
  }

  // Start gif from the first frame.
  next_frame_index_ = 0;
  UpdateFrame();
  RecordFetchFramesTime();
}

void PickerGifView::OnPreviewImageFetched(const gfx::ImageSkia& preview_image) {
  // Only show preview image if gif frames have not already been fetched.
  if (frames_.empty()) {
    SetImage(ui::ImageModel::FromImageSkia(preview_image));

    if (!preview_image.isNull()) {
      RecordFetchFramesTime();
    }
  }
}

void PickerGifView::RecordFetchFramesTime() {
  if (fetch_frames_start_time_.has_value()) {
    UmaHistogramCustomTimes("Ash.Picker.TimeToFirstGifFrame",
                            base::TimeTicks::Now() - *fetch_frames_start_time_,
                            base::Milliseconds(0), base::Seconds(1),
                            /*buckets=*/50);
    fetch_frames_start_time_ = std::nullopt;
  }
}

BEGIN_METADATA(PickerGifView)
END_METADATA

}  // namespace ash
