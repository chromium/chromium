// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_IMAGE_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_IMAGE_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/files/file.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// A wrapper around a `gfx::ImageSkia` that supports dynamic updates.
class ASH_PUBLIC_EXPORT HoldingSpaceImage {
 public:
  using CallbackList = base::RepeatingClosureList;

  // Returns a bitmap.
  using BitmapCallback =
      base::OnceCallback<void(const SkBitmap* bitmap, base::File::Error error)>;

  // Returns a bitmap asynchronously for a given size.
  using AsyncBitmapResolver =
      base::RepeatingCallback<void(const base::FilePath& file_path,
                                   const gfx::Size& size,
                                   BitmapCallback callback)>;

  HoldingSpaceImage(const gfx::Size& max_size,
                    const base::FilePath& backing_file_path,
                    AsyncBitmapResolver async_bitmap_resolver);
  HoldingSpaceImage(const HoldingSpaceImage&) = delete;
  HoldingSpaceImage& operator=(const HoldingSpaceImage&) = delete;
  ~HoldingSpaceImage();

  // Returns the maximum size required for a holding space item of `type`.
  static gfx::Size GetMaxSizeForType(HoldingSpaceItem::Type type);

  static void SetUseZeroInvalidationDelayForTesting(bool value);

  bool operator==(const HoldingSpaceImage& rhs) const;

  // Adds `callback` to be notified of changes to the underlying `image_skia_`.
  base::CallbackListSubscription AddImageSkiaChangedCallback(
      CallbackList::CallbackType callback) const;

  // Returns the underlying `gfx::ImageSkia`. If `size` is omitted, the
  // `max_size_` passed in the constructor is used. If `size` is present, it
  // must be less than or equal to the `max_size_` passed in the constructor in
  // order to prevent pixelation that would otherwise occur due to upscaling.
  // Note that the image source may be dynamically updated, so UI classes should
  // observe and react to updates.
  gfx::ImageSkia GetImageSkia(
      const base::Optional<gfx::Size>& size = base::nullopt) const;

  // Creates new image skia for the item, and thus invalidates currently loaded
  // representation. When the image is requested next time, the image
  // representations will be reloaded.
  void Invalidate();

  // Updates the backing file path that should be used to generate image
  // representations for the item. The method will *not* invalidate previously
  // loaded image representations - it assumes that the file contents remained
  // the same, and old representations are thus still valid.
  void UpdateBackingFilePath(const base::FilePath& file_path);

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
  base::Optional<base::File::Error> async_bitmap_resolver_error_;

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
