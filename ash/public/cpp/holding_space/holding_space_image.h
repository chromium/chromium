// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_IMAGE_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_IMAGE_H_

#include <optional>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/callback_list.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// TODO(crbug.com/40173943): Rename and move to more generic location.
// A wrapper around a `gfx::ImageSkia` that supports dynamic updates.
class ASH_PUBLIC_EXPORT HoldingSpaceImage {
 public:
  using CallbackList = base::RepeatingClosureList;

  // Returns an `SkBitmap`.
  using BitmapCallback =
      base::OnceCallback<void(const SkBitmap* bitmap, base::File::Error error)>;

  // Returns an `SkBitmap` asynchronously for a given `file_path` and `size`.
  using AsyncBitmapResolver =
      base::RepeatingCallback<void(const base::FilePath& file_path,
                                   const gfx::Size& size,
                                   BitmapCallback callback)>;

  // Returns a `gfx::ImageSkia` to be used as a placeholder prior to the async
  // `SkBitmap` being resolved or when async `SkBitmap` resolution fails.
  // NOTE: The placeholder resolver will be invoked during `HoldingSpaceImage`
  // construction, at which time `dark_background` and `is_folder` are absent.
  using PlaceholderImageSkiaResolver = base::RepeatingCallback<gfx::ImageSkia(
      const base::FilePath& file_path,
      const gfx::Size& size,
      const std::optional<bool>& dark_background,
      const std::optional<bool>& is_folder)>;

  HoldingSpaceImage(const gfx::Size& max_size,
                    const base::FilePath& backing_file_path,
                    AsyncBitmapResolver async_bitmap_resolver);

  HoldingSpaceImage(
      const gfx::Size& max_size,
      const base::FilePath& backing_file_path,
      AsyncBitmapResolver async_bitmap_resolver,
      PlaceholderImageSkiaResolver placeholder_image_skia_resolver);

  HoldingSpaceImage(const HoldingSpaceImage&) = delete;
  HoldingSpaceImage& operator=(const HoldingSpaceImage&) = delete;
  ~HoldingSpaceImage();

  // Returns a placeholder resolver which creates an image corresponding to the
  // file type of the provided `file_path`.
  static PlaceholderImageSkiaResolver CreateDefaultPlaceholderImageSkiaResolver(
      bool use_light_mode_as_default = false);

  // Sets whether image invalidation should be done without delay. This makes it
  // possible to disable invalidation throttling that reduces the number of
  // async bitmap requests in production.
  static void SetUseZeroInvalidationDelayForTesting(bool value);

  bool operator==(const HoldingSpaceImage& rhs) const;

  // Adds `callback` to be notified of changes to the underlying `image_skia_`.
  base::CallbackListSubscription AddImageSkiaChangedCallback(
      CallbackList::CallbackType callback) const;

  // Returns the underlying `gfx::ImageSkia`. If `size` is omitted, the
  // `max_size_` passed in the constructor is used. If `size` is present, it
  // must be less than or equal to the `max_size_` passed in the constructor in
  // order to prevent pixelation that would otherwise occur due to upscaling. If
  // `dark_background` is `true`, file type icons will use lighter foreground
  // colors to ensure sufficient contrast. Note that the image source may be
  // dynamically updated, so UI classes should observe and react to updates.
  gfx::ImageSkia GetImageSkia(
      const std::optional<gfx::Size>& size = std::nullopt,
      bool dark_background = false) const;

  // Creates new image skia for the item, and thus invalidates currently loaded
  // representation. When the image is requested next time, the image
  // representations will be reloaded.
  void Invalidate();

  // Updates the backing file path that should be used to generate image
  // representations for the item. The method will *not* invalidate previously
  // loaded image representations - it assumes that the file contents remained
  // the same, and old representations are thus still valid.
  void UpdateBackingFilePath(const base::FilePath& file_path);

  // Whether the image currently uses the placeholder image skia. True if no
  // bitmaps have been successfully resolved.
  bool UsingPlaceholder() const;

  // Fires the image invalidation timer if it's currently running.
  bool FireInvalidateTimerForTesting();

 private:
  class ImageSkiaSource;

  // Requests a load for the image representation for the provided scale.
  void LoadBitmap(float scale);

  // Response to an image representation load request.
  void OnBitmapLoaded(const base::FilePath& file_path,
                      float scale,
                      const SkBitmap* bitmap,
                      base::File::Error error);

  // Creates `image_skia_` to be used for the holding space image.
  // If `image_skia_` already exists, it will be recreated with a fresh image
  // source.
  void CreateImageSkia();

  // `Invalidate()` requests are handled with a delay to reduce number of image
  // loads if the backing file gets updated multiple times in quick succession.
  void OnInvalidateTimer();

  const gfx::Size max_size_;
  base::FilePath backing_file_path_;
  AsyncBitmapResolver async_bitmap_resolver_;
  std::optional<base::File::Error> async_bitmap_resolver_error_;
  PlaceholderImageSkiaResolver placeholder_image_skia_resolver_;

  gfx::ImageSkia image_skia_;
  gfx::ImageSkia placeholder_;

  // Timer used to throttle image invalidate requests.
  base::OneShotTimer invalidate_timer_;

  // Mutable to allow const access from `AddImageSkiaChangedCallback()`.
  mutable CallbackList callback_list_;

  base::WeakPtrFactory<HoldingSpaceImage> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_IMAGE_H_
