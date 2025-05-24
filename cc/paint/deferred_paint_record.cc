// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/deferred_paint_record.h"

#include "base/notimplemented.h"
#include "cc/paint/paint_record.h"

namespace cc {

bool DeferredPaintRecord::KnownToBeOpaque() const {
  return false;
}

bool DeferredPaintRecord::IsPaintWorkletInput() const {
  return false;
}

bool DeferredPaintRecord::NeedsLayer() const {
  return false;
}

}  // namespace cc
