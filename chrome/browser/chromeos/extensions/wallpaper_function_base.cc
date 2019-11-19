// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"

#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/lazy_task_runner.h"
#include "base/task/task_traits.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/login_state/login_state.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
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

const int kWallpaperLayoutCount = base::size(kWallpaperLayoutArrays);

base::LazySequencedTaskRunner g_blocking_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::ThreadPool(),
                         base::MayBlock(),
                         base::TaskPriority::USER_BLOCKING,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));
base::LazySequencedTaskRunner g_non_blocking_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::ThreadPool(),
                         base::MayBlock(),
                         base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));

// Returns an image of |size| that contains as much of |image| as possible
// without distorting the |image|.  Unused areas are cropped away.
gfx::ImageSkia ScaleAspectRatioAndCropCenter(const gfx::Size& size,
                                             const gfx::ImageSkia& image) {
  float scale = std::min(float{image.width()} / float{size.width()},
                         float{image.height()} / float{size.height()});
  gfx::Size scaled_size = {scale * size.width(), scale * size.height()};
  gfx::Rect bounds{{0, 0}, image.size()};
  bounds.ClampToCenteredSize(scaled_size);
  auto scaled_and_cropped_image = gfx::ImageSkiaOperations::CreateTiledImage(
      image, bounds.x(), bounds.y(), bounds.width(), bounds.height());
  return gfx::ImageSkiaOperations::CreateResizedImage(
      scaled_and_cropped_image, skia::ImageOperations::RESIZE_LANCZOS3, size);
}

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

}  // namespace wallpaper_api_util

class WallpaperFunctionBase::UnsafeWallpaperDecoder
    : public ImageDecoder::ImageRequest {
 public:
  explicit UnsafeWallpaperDecoder(scoped_refptr<WallpaperFunctionBase> function)
      : function_(function) {}

  void Start(const std::vector<uint8_t>& image_data) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // This function can only be called after user login. It is fine to use
    // unsafe image decoder here. Before user login, a robust jpeg decoder will
    // be used.
    CHECK(chromeos::LoginState::Get()->IsUserLoggedIn());
    std::string image_data_str(image_data.begin(), image_data.end());
    ImageDecoder::StartWithOptions(this, image_data_str,
                                   ImageDecoder::DEFAULT_CODEC, true);
  }

  void Cancel() {
    cancel_flag_.Set();
  }

  void OnImageDecoded(const SkBitmap& decoded_image) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Make the SkBitmap immutable as we won't modify it. This is important
    // because otherwise it gets duplicated during painting, wasting memory.
    SkBitmap immutable(decoded_image);
    immutable.setImmutable();
    gfx::ImageSkia final_image = gfx::ImageSkia::CreateFrom1xBitmap(immutable);
    final_image.MakeThreadSafe();
    if (cancel_flag_.IsSet()) {
      function_->OnCancel();
      delete this;
      return;
    }
    function_->OnWallpaperDecoded(final_image);
    delete this;
  }

  void OnDecodeImageFailed() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    function_->OnFailure(
        l10n_util::GetStringUTF8(IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER));
    delete this;
  }

 private:
  scoped_refptr<WallpaperFunctionBase> function_;
  base::AtomicFlag cancel_flag_;

  DISALLOW_COPY_AND_ASSIGN(UnsafeWallpaperDecoder);
};

WallpaperFunctionBase::UnsafeWallpaperDecoder*
    WallpaperFunctionBase::unsafe_wallpaper_decoder_;

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
  if (unsafe_wallpaper_decoder_)
    unsafe_wallpaper_decoder_->Cancel();
  unsafe_wallpaper_decoder_ = new UnsafeWallpaperDecoder(this);
  unsafe_wallpaper_decoder_->Start(data);
}

void WallpaperFunctionBase::OnCancel() {
  unsafe_wallpaper_decoder_ = nullptr;
  Respond(Error(wallpaper_api_util::kCancelWallpaperMessage));
}

void WallpaperFunctionBase::OnFailure(const std::string& error) {
  unsafe_wallpaper_decoder_ = nullptr;
  Respond(Error(error));
}

void WallpaperFunctionBase::GenerateThumbnail(
    const gfx::ImageSkia& image,
    const gfx::Size& size,
    scoped_refptr<base::RefCountedBytes>* thumbnail_data_out) {
  *thumbnail_data_out = new base::RefCountedBytes();
  gfx::JPEGCodec::Encode(
      *wallpaper_api_util::ScaleAspectRatioAndCropCenter(size, image).bitmap(),
      90 /*quality=*/, &(*thumbnail_data_out)->data());
}
