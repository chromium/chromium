// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/cross_thread_shared_bitmap.h"

#include <utility>

namespace cc {

CrossThreadSharedBitmap::CrossThreadSharedBitmap(
    const viz::SharedBitmapId& id,
    base::ReadOnlySharedMemoryRegion region,
    base::WritableSharedMemoryMapping mapping,
    const gfx::Size& size,
    viz::SharedImageFormat format)
    : id_(id),
      region_(std::move(region)),
      mapping_(std::move(mapping)),
      size_(size),
      format_(format) {}

CrossThreadSharedBitmap::~CrossThreadSharedBitmap() = default;

}  // namespace cc
