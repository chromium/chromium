// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_UI_RESOURCE_BITMAP_H_
#define CC_RESOURCES_UI_RESOURCE_BITMAP_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/geometry/size.h"

class SkBitmap;

namespace cc {

// A bitmap class that contains a ref-counted reference to a SkPixelRef that
// holds the content of the bitmap (cannot use SkBitmap because of ETC1).
// Thread-safety (by ways of SkPixelRef) ensures that both main and impl threads
// can hold references to the bitmap and that asynchronous uploads are allowed.
class CC_EXPORT UIResourceBitmap {
 public:
  enum UIResourceFormat {
    RGBA8,
    ALPHA_8,
    ETC1
  };

  gfx::Size GetSize() const { return gfx::Size(info_.width(), info_.height()); }
  UIResourceFormat GetFormat() const { return format_; }
  bool GetOpaque() const { return info_.isOpaque(); }

  // Draw the UIResourceBitmap onto the provided |canvas| using the style
  // information specified by |paint|.
  void DrawToCanvas(SkCanvas* canvas, SkPaint* paint);

  // User must ensure that |skbitmap| is immutable.  The SkBitmap Format should
  // be 32-bit RGBA or 8-bit ALPHA.
  explicit UIResourceBitmap(const SkBitmap& skbitmap);
  UIResourceBitmap(const gfx::Size& size, bool is_opaque);
  UIResourceBitmap(sk_sp<SkPixelRef> pixel_ref, const gfx::Size& size);
  UIResourceBitmap(const UIResourceBitmap& other);
  ~UIResourceBitmap();

  base::span<const uint8_t> GetPixels() const;
  size_t SizeInBytes() const;
  size_t row_bytes() const { return pixel_ref_ ? pixel_ref_->rowBytes() : 0; }

  bool IsUniquelyOwned() const { return pixel_ref_->unique(); }

  SkBitmap GetBitmapForTesting() const;

 private:
  friend class AutoLockUIResourceBitmap;

  void Create(sk_sp<SkPixelRef> pixel_ref,
              const SkImageInfo& info,
              UIResourceFormat format);

  sk_sp<SkPixelRef> pixel_ref_;
  UIResourceFormat format_;
  SkImageInfo info_;
};

}  // namespace cc

#endif  // CC_RESOURCES_UI_RESOURCE_BITMAP_H_
