// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DEFERRED_PAINT_RECORD_H_
#define CC_PAINT_DEFERRED_PAINT_RECORD_H_

#include <cstddef>

#include "base/memory/ref_counted.h"
#include "cc/paint/paint_export.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

class CC_PAINT_EXPORT DeferredPaintRecord
    : public base::RefCountedThreadSafe<DeferredPaintRecord> {
 public:
  virtual bool IsPaintWorkletInput() const;
  virtual gfx::SizeF GetSize() const = 0;
  virtual bool NeedsLayer() const;

  // True if all the animated frames are opaque. Can be false only if animated
  // frames are colors.
  virtual bool KnownToBeOpaque() const;

 protected:
  friend class base::RefCountedThreadSafe<DeferredPaintRecord>;
  virtual ~DeferredPaintRecord() = default;
};

}  // namespace cc

#endif  // CC_PAINT_DEFERRED_PAINT_RECORD_H_
