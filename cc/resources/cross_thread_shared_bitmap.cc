// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/cross_thread_shared_bitmap.h"

namespace cc {

CrossThreadSharedBitmap::CrossThreadSharedBitmap(const viz::SharedBitmapId& id,
                                                 base::MappedReadOnlyRegion shm,
                                                 const gfx::Size& size,
                                                 viz::ResourceFormat format)
    : id_(id),
      region_(std::move(shm.region)),
      mapping_(std::move(shm.mapping)),
      size_(size),
      format_(format) {}

CrossThreadSharedBitmap::~CrossThreadSharedBitmap() = default;

}  // namespace cc
