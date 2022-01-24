// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator.h"

#include <string>
#include <utility>

#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator_observer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_extraction_result.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_runner.h"
#include "base/task/task_runner_util.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"

using LumaRange = color_utils::LumaRange;
using SaturationRange = color_utils::SaturationRange;

namespace ash {

namespace {

// The largest image size, in pixels, to synchronously calculate the prominent
// color. This is a simple heuristic optimization because extraction on images
// smaller than this should run very quickly, and offloading the task to another
// thread would actually take longer.
const int kMaxPixelsForSynchronousCalculation = 100;

// Wrapper for color_utils::CalculateProminentColorsOfBitmap() that records
// wallpaper specific metrics.
//
// NOTE: |image| is intentionally a copy to ensure it exists for the duration of
// the calculation.
std::vector<SkColor> CalculateWallpaperColor(
    const gfx::ImageSkia image,
    const std::vector<color_utils::ColorProfile> color_profiles) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  const std::vector<color_utils::Swatch> prominent_swatches =
      color_utils::CalculateProminentColorsOfBitmap(
          *image.bitmap(), color_profiles, nullptr /* region */,
          color_utils::ColorSwatchFilter());

  std::vector<SkColor> prominent_colors(prominent_swatches.size());
  for (size_t i = 0; i < prominent_swatches.size(); ++i)
    prominent_colors[i] = prominent_swatches[i].color;

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

  return prominent_colors;
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
  prominent_colors_ =
      std::vector<SkColor>(color_profiles_.size(), SK_ColorTRANSPARENT);
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
    const std::vector<SkColor> prominent_colors =
        CalculateWallpaperColor(image_, color_profiles_);
    NotifyCalculationComplete(prominent_colors);
    return true;
  }

  image_.MakeThreadSafe();
  if (base::PostTaskAndReplyWithResult(
          task_runner_.get(), FROM_HERE,
          base::BindOnce(&CalculateWallpaperColor, image_, color_profiles_),
          base::BindOnce(&WallpaperColorCalculator::OnAsyncCalculationComplete,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::TimeTicks::Now()))) {
    return true;
  }

  LOG(WARNING) << "PostSequencedWorkerTask failed. "
               << "Wallpaper prominent colors may not be calculated.";

  prominent_colors_ =
      std::vector<SkColor>(color_profiles_.size(), SK_ColorTRANSPARENT);
  return false;
}

void WallpaperColorCalculator::SetTaskRunnerForTest(
    scoped_refptr<base::TaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void WallpaperColorCalculator::OnAsyncCalculationComplete(
    base::TimeTicks async_start_time,
    const std::vector<SkColor>& prominent_colors) {
  UMA_HISTOGRAM_TIMES("Ash.Wallpaper.ColorExtraction.UserDelay",
                      base::TimeTicks::Now() - async_start_time);
  NotifyCalculationComplete(prominent_colors);
}

void WallpaperColorCalculator::NotifyCalculationComplete(
    const std::vector<SkColor>& prominent_colors) {
  prominent_colors_ = prominent_colors;
  for (auto& observer : observers_)
    observer.OnColorCalculationComplete();

  // This could be deleted!
}

}  // namespace ash
