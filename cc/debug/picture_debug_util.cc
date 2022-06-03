// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/picture_debug_util.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/base64.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace cc {

void PictureDebugUtil::SerializeAsBase64(const SkPicture* picture,
                                         std::string* output) {
  sk_sp<SkData> data = picture->serialize();
  base::Base64Encode(
      base::StringPiece(static_cast<const char*>(data->data()), data->size()),
      output);
}

}  // namespace cc
