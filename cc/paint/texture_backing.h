// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TEXTURE_BACKING_H_
#define CC_PAINT_TEXTURE_BACKING_H_

#include "base/memory/ref_counted.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace gpu {
struct Mailbox;
}  // namespace gpu

namespace cc {

class CC_PAINT_EXPORT TextureBackingContext
    : public base::RefCountedThreadSafe<TextureBackingContext> {
 protected:
  friend class base::RefCountedThreadSafe<TextureBackingContext>;
  virtual ~TextureBackingContext() = default;
};

// Used for storing mailboxes in a PaintImage.
// This class must be created, used and destroyed on the same thread.
class CC_PAINT_EXPORT TextureBacking : public SkRefCnt {
 public:
  TextureBacking() = default;
  TextureBacking(const TextureBacking&) = delete;
  ~TextureBacking() override = default;

  TextureBacking& operator=(const TextureBacking&) = delete;

  // Returns the metadata for the associated texture.
  virtual const SkImageInfo& GetSkImageInfo() = 0;

  // Returns the shared image mailbox backing for this texture.
  virtual gpu::Mailbox GetMailbox() const = 0;

  virtual void Bind(scoped_refptr<TextureBackingContext>) = 0;
  virtual void Unbind() = 0;

  // Gets SkImage via a readback from GPU memory. Use this when actual SkImage
  // pixel data is required in software.
  virtual sk_sp<SkImage> GetSkImageViaReadback() = 0;

  // Read texture's pixels, starting at (`src_x`, `src_y`), into the caller
  // owned memory referenced by `dst_pixmap`.
  virtual bool readPixels(const SkPixmap& dst_pixmap, int src_x, int src_y) = 0;
};

}  // namespace cc

#endif  // CC_PAINT_TEXTURE_BACKING_H_
