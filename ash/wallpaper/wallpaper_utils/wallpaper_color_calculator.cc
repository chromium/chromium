// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator.h"

#include <string>
#include <utility>

#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator_observer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_extraction_result.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_runner.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using LumaRange = color_utils::LumaRange;
using SaturationRange = color_utils::SaturationRange;

namespace ash {

namespace {

// The largest image size, in pixels, to synchronously calculate the prominent
// color. This is a simple heuristic optimization because extraction on images
// smaller than this should run very quickly, and offloading the task to another
// thread would actually take longer.
const int kMaxPixelsForSynchronousCalculation = 100;

// Specifies the size of the resized image used to calculate the wallpaper
// colors.
constexpr int kWallpaperSizeForColorCalculation = 256;

const gfx::ImageSkia GetResizedImage(const gfx::ImageSkia& image) {
  if (std::max(image.width(), image.height()) <
      kWallpaperSizeForColorCalculation) {
    return image;
  }

  // Resize the image maintaining our aspect ratio.
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = kWallpaperSizeForColorCalculation;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > kWallpaperSizeForColorCalculation) {
    width = kWallpaperSizeForColorCalculation;
    height = static_cast<int>(width / aspect_ratio);
  }
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_GOOD, gfx::Size(width, height));
}

// Wrapper for color_utils::CalculateProminentColorsOfBitmap() and
// color_utils::CalculateKMeanColorOfBitmap that records wallpaper specific
// metrics. Note, |image| is resized to |kWallpaperSizeForColorCalculation|
// to speed up the calculation.
//
// NOTE: |image| is intentionally a copy to ensure it exists for the duration of
// the calculation.
WallpaperCalculatedColors CalculateWallpaperColor(
    const gfx::ImageSkia image,
    const std::vector<color_utils::ColorProfile> color_profiles) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  gfx::ImageSkia resized_image = GetResizedImage(image);
  const std::vector<color_utils::Swatch> prominent_swatches =
      color_utils::CalculateProminentColorsOfBitmap(
          *resized_image.bitmap(), color_profiles, nullptr /* region */,
          color_utils::ColorSwatchFilter());

  std::vector<SkColor> prominent_colors(prominent_swatches.size());
  for (size_t i = 0; i < prominent_swatches.size(); ++i)
    prominent_colors[i] = prominent_swatches[i].color;

  constexpr color_utils::HSL kNoBounds = {-1, -1, -1};
  SkColor k_mean_color = color_utils::CalculateKMeanColorOfBitmap(
      *resized_image.bitmap(), resized_image.height(), kNoBounds, kNoBounds,
      /*find_closest=*/true);

  UMA_HISTOGRAM_TIMES("Ash.Wallpaper.ColorExtraction.Durations",
                      base::TimeTicks::Now() - start_time);
  WallpaperColorExtractionResult result = NUM_COLOR_EXTRACTION_RESULTS;
  for (size_t i = 0; i < color_profiles.size(); ++i) {
    bool is_result_transparent = prominent_colors[i] == SK_ColorTRANSPARENT;
    if (color_profiles[i].saturation == SaturationRange::VIBRANT) {
      switch (color_profiles[i].luma) {
        case LumaRange::ANY:
          // There should be no color profiles with the ANY luma range.
          NOTREACHED();
          break;
        case LumaRange::DARK:
          result = is_result_transparent ? RESULT_DARK_VIBRANT_TRANSPARENT
                                         : RESULT_DARK_VIBRANT_OPAQUE;
          break;
        case LumaRange::NORMAL:
          result = is_result_transparent ? RESULT_NORMAL_VIBRANT_TRANSPARENT
                                         : RESULT_NORMAL_VIBRANT_OPAQUE;
          break;
        case LumaRange::LIGHT:
          result = is_result_transparent ? RESULT_LIGHT_VIBRANT_TRANSPARENT
                                         : RESULT_LIGHT_VIBRANT_OPAQUE;
          break;
      }
    } else {
      switch (color_profiles[i].luma) {
        case LumaRange::ANY:
          // There should be no color profiles with the ANY luma range.
          NOTREACHED();
          break;
        case LumaRange::DARK:
          result = is_result_transparent ? RESULT_DARK_MUTED_TRANSPARENT
                                         : RESULT_DARK_MUTED_OPAQUE;
          break;
        case LumaRange::NORMAL:
          result = is_result_transparent ? RESULT_NORMAL_MUTED_TRANSPARENT
                                         : RESULT_NORMAL_MUTED_OPAQUE;
          break;
        case LumaRange::LIGHT:
          result = is_result_transparent ? RESULT_LIGHT_MUTED_TRANSPARENT
                                         : RESULT_LIGHT_MUTED_OPAQUE;
          break;
      }
    }
  }
  DCHECK_NE(NUM_COLOR_EXTRACTION_RESULTS, result);
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.ColorExtractionResult2", result,
                            NUM_COLOR_EXTRACTION_RESULTS);
  return WallpaperCalculatedColors(prominent_colors, k_mean_color);
}

bool ShouldCalculateSync(const gfx::ImageSkia& image) {
  return image.width() * image.height() <= kMaxPixelsForSynchronousCalculation;
}

}  // namespace

WallpaperColorCalculator::WallpaperColorCalculator(
    const gfx::ImageSkia& image,
    const std::vector<color_utils::ColorProfile>& color_profiles,
    scoped_refptr<base::TaskRunner> task_runner)
    : image_(image),
      color_profiles_(color_profiles),
      task_runner_(std::move(task_runner)) {
  std::vector<SkColor> prominent_colors =
      std::vector<SkColor>(color_profiles.size(), SK_ColorTRANSPARENT);
  calculated_colors_ =
      WallpaperCalculatedColors(prominent_colors, SK_ColorTRANSPARENT);
}

WallpaperColorCalculator::~WallpaperColorCalculator() = default;

void WallpaperColorCalculator::AddObserver(
    WallpaperColorCalculatorObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperColorCalculator::RemoveObserver(
    WallpaperColorCalculatorObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WallpaperColorCalculator::StartCalculation() {
  if (ShouldCalculateSync(image_)) {
    NotifyCalculationComplete(CalculateWallpaperColor(image_, color_profiles_));
    return true;
  }

  image_.MakeThreadSafe();
  if (task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CalculateWallpaperColor, image_, color_profiles_),
          base::BindOnce(&WallpaperColorCalculator::OnAsyncCalculationComplete,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::TimeTicks::Now()))) {
    return true;
  }

  LOG(WARNING) << "PostSequencedWorkerTask failed. "
               << "Wallpaper prominent colors may not be calculated.";

  calculated_colors_.prominent_colors =
      std::vector<SkColor>(color_profiles_.size(), SK_ColorTRANSPARENT);
  calculated_colors_.k_mean_color = SK_ColorTRANSPARENT;
  return false;
}

void WallpaperColorCalculator::SetTaskRunnerForTest(
    scoped_refptr<base::TaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void WallpaperColorCalculator::OnAsyncCalculationComplete(
    base::TimeTicks async_start_time,
    const WallpaperCalculatedColors& calculated_colors) {
  UMA_HISTOGRAM_TIMES("Ash.Wallpaper.ColorExtraction.UserDelay",
                      base::TimeTicks::Now() - async_start_time);
  NotifyCalculationComplete(calculated_colors);
}

void WallpaperColorCalculator::NotifyCalculationComplete(
    const WallpaperCalculatedColors& calculated_colors) {
  calculated_colors_ = calculated_colors;
  for (auto& observer : observers_)
    observer.OnColorCalculationComplete();

  // This could be deleted!
}

}  // namespace ash
