// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_IMAGE_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_IMAGE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// A wrapper around a `gfx::ImageSkia` that supports dynamic updates. When
// updates occur or an instance is being destroyed, observers are notified.
class ASH_PUBLIC_EXPORT HoldingSpaceImage {
 public:
  // An observer which receives notifications of `HoldingSpaceImage` events.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the `HoldingSpaceImage` is updated. UI classes should react
    // to this event by invalidating any associated views.
    virtual void OnHoldingSpaceImageUpdated(const HoldingSpaceImage*) {}

    // Invoked when the `HoldingSpaceImage` is being destroyed. Any observers
    // should react to this event by unregistering from the observer list.
    virtual void OnHoldingSpaceImageDestroying(const HoldingSpaceImage*) {}
  };

  // Returns a bitmap.
  using BitmapCallback = base::OnceCallback<void(const SkBitmap*)>;

  // Returns a bitmap asynchronously for a given size.
  using AsyncBitmapResolver =
      base::RepeatingCallback<void(const gfx::Size&, BitmapCallback)>;

  HoldingSpaceImage(const gfx::ImageSkia& placeholder,
                    AsyncBitmapResolver async_bitmap_resolver);
  HoldingSpaceImage(const HoldingSpaceImage&) = delete;
  HoldingSpaceImage& operator=(const HoldingSpaceImage&) = delete;
  ~HoldingSpaceImage();

  bool operator==(const HoldingSpaceImage& rhs) const;

  // Adds/remove the specified `observer`.
  void AddObserver(Observer* observer) const;
  void RemoveObserver(Observer* observer) const;

  // Returns the underlying `gfx::ImageSkia`. Note that the image source may be
  // dynamically updated, so UI classes should observe and react to updates.
  const gfx::ImageSkia& image_skia() const { return image_skia_; }

 private:
  class ImageSkiaSource;

  void NotifyDestroying();
  void NotifyUpdated(float scale);

  gfx::ImageSkia image_skia_;

  // Mutable to allow const access from `AddObserver()`/`RemoveObserver()`.
  mutable base::ObserverList<HoldingSpaceImage::Observer> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_IMAGE_H_
