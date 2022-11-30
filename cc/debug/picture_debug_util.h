// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_PICTURE_DEBUG_UTIL_H_
#define CC_DEBUG_PICTURE_DEBUG_UTIL_H_

#include <string>

#include "cc/debug/debug_export.h"

class SkPicture;

namespace cc {

class CC_DEBUG_EXPORT PictureDebugUtil {
 public:
  static void SerializeAsBase64(const SkPicture* picture, std::string* output);
};

}  // namespace cc

#endif  // CC_DEBUG_PICTURE_DEBUG_UTIL_H_
