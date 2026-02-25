// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_
#define CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_

#include <stddef.h>

#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT ImageDecodeCacheUtils {
 public:
  // Returns budget bytes for decoded images that may be different depending
  // whether it's for renderer or for the ui compositor.
  static size_t GetWorkingSetBytesForImageDecode(bool for_renderer);
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_
