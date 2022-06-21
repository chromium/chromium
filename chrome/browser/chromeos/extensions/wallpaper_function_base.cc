// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/task_traits.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/login_state/login_state.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_operations.h"

using content::BrowserThread;

namespace wallpaper_api_util {
namespace {

// Keeps in sync (same order) with WallpaperLayout enum in header file.
const char* const kWallpaperLayoutArrays[] = {
  "CENTER",
  "CENTER_CROPPED",
  "STRETCH",
  "TILE"
};

const int kWallpaperLayoutCount = std::size(kWallpaperLayoutArrays);

base::LazyThreadPoolSequencedTaskRunner g_blocking_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::USER_BLOCKING,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));
base::LazyThreadPoolSequencedTaskRunner g_non_blocking_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));

// Returns an image of |size| that contains as much of |image| as possible
// without distorting the |image|.  Unused areas are cropped away.
gfx::ImageSkia ScaleAspectRatioAndCropCenter(const gfx::Size& size,
                                             const gfx::ImageSkia& image) {
  float scale = std::min(static_cast<float>(image.width()) / size.width(),
                         static_cast<float>(image.height()) / size.height());
  gfx::Size scaled_size = {base::ClampFloor(scale * size.width()),
                           base::ClampFloor(scale * size.height())};
  gfx::Rect bounds{{0, 0}, image.size()};
  bounds.ClampToCenteredSize(scaled_size);
  auto scaled_and_cropped_image = gfx::ImageSkiaOperations::CreateTiledImage(
      image, bounds.x(), bounds.y(), bounds.width(), bounds.height());
  return gfx::ImageSkiaOperations::CreateResizedImage(
      scaled_and_cropped_image, skia::ImageOperations::RESIZE_LANCZOS3, size);
}

const int kThumbnailEncodeQuality = 90;

}  // namespace

const char kCancelWallpaperMessage[] = "Set wallpaper was canceled.";

ash::WallpaperLayout GetLayoutEnum(const std::string& layout) {
  for (int i = 0; i < kWallpaperLayoutCount; i++) {
    if (layout.compare(kWallpaperLayoutArrays[i]) == 0)
      return static_cast<ash::WallpaperLayout>(i);
  }
  // Default to use CENTER layout.
  return ash::WALLPAPER_LAYOUT_CENTER;
}

std::string GetLayoutString(const ash::WallpaperLayout& layout) {
  return kWallpaperLayoutArrays[layout >= ash::NUM_WALLPAPER_LAYOUT ? 0
                                                                    : layout];
}

void RecordCustomWallpaperLayout(const ash::WallpaperLayout& layout) {
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.CustomLayout", layout,
                            ash::NUM_WALLPAPER_LAYOUT);
}

std::vector<uint8_t> GenerateThumbnail(const gfx::ImageSkia& image,
                                       const gfx::Size& size) {
  std::vector<uint8_t> data_out;
  gfx::JPEGCodec::Encode(
      *wallpaper_api_util::ScaleAspectRatioAndCropCenter(size, image).bitmap(),
      kThumbnailEncodeQuality, &data_out);
  return data_out;
}

WallpaperDecoder::WallpaperDecoder(DecodedCallback decoded_cb,
                                   CanceledCallback canceled_cb,
                                   FailedCallback failed_cb)
    : decoded_cb_(std::move(decoded_cb)),
      canceled_cb_(std::move(canceled_cb)),
      failed_cb_(std::move(failed_cb)) {}

WallpaperDecoder::~WallpaperDecoder() = default;

void WallpaperDecoder::Start(const std::vector<uint8_t>& image_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CHECK(chromeos::LoginState::Get()->IsUserLoggedIn());
  ImageDecoder::StartWithOptions(this, image_data, ImageDecoder::DEFAULT_CODEC,
                                 true);
}

void WallpaperDecoder::Cancel() {
  cancel_flag_.Set();
}

void WallpaperDecoder::OnImageDecoded(const SkBitmap& decoded_image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  SkBitmap immutable(decoded_image);
  immutable.setImmutable();
  gfx::ImageSkia final_image = gfx::ImageSkia::CreateFrom1xBitmap(immutable);
  final_image.MakeThreadSafe();
  if (cancel_flag_.IsSet()) {
    std::move(canceled_cb_).Run();
    delete this;
    return;
  }
  std::move(decoded_cb_).Run(final_image);
  delete this;
}

void WallpaperDecoder::OnDecodeImageFailed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(failed_cb_)
      .Run(l10n_util::GetStringUTF8(IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER));
  delete this;
}

}  // namespace wallpaper_api_util

wallpaper_api_util::WallpaperDecoder*
    WallpaperFunctionBase::wallpaper_decoder_ = nullptr;

const int WallpaperFunctionBase::kWallpaperThumbnailWidth = 108;
const int WallpaperFunctionBase::kWallpaperThumbnailHeight = 68;

WallpaperFunctionBase::WallpaperFunctionBase() = default;

WallpaperFunctionBase::~WallpaperFunctionBase() = default;

base::SequencedTaskRunner* WallpaperFunctionBase::GetBlockingTaskRunner() {
  return wallpaper_api_util::g_blocking_task_runner.Get().get();
}

base::SequencedTaskRunner* WallpaperFunctionBase::GetNonBlockingTaskRunner() {
  return wallpaper_api_util::g_non_blocking_task_runner.Get().get();
}

void WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
    base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(task_runner->RunsTasksInCurrentSequence());
#endif
}

void WallpaperFunctionBase::StartDecode(const std::vector<uint8_t>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (wallpaper_decoder_)
    wallpaper_decoder_->Cancel();
  wallpaper_decoder_ = new wallpaper_api_util::WallpaperDecoder(
      base::BindOnce(&WallpaperFunctionBase::OnWallpaperDecoded, this),
      base::BindOnce(&WallpaperFunctionBase::OnCancel, this),
      base::BindOnce(&WallpaperFunctionBase::OnFailure, this));
  wallpaper_decoder_->Start(data);
}

void WallpaperFunctionBase::OnCancel() {
  wallpaper_decoder_ = nullptr;
  Respond(Error(wallpaper_api_util::kCancelWallpaperMessage));
}

void WallpaperFunctionBase::OnFailure(const std::string& error) {
  wallpaper_decoder_ = nullptr;
  Respond(Error(error));
}
