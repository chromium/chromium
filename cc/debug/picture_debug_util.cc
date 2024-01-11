// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/picture_debug_util.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace cc {

void PictureDebugUtil::SerializeAsBase64(const SkPicture* picture,
                                         std::string* output) {
  SkSerialProcs procs{.fImageProc = [](SkImage* img, void*) -> sk_sp<SkData> {
    // Note: if the picture contains texture-backed (gpu) images, they will fail
    // to be read-back and therefore fail to be encoded unless we can thread the
    // correct GrDirectContext through to here.
    return SkPngEncoder::Encode(nullptr, img, SkPngEncoder::Options{});
  }};
  sk_sp<SkData> data = picture->serialize(&procs);
  *output = base::Base64Encode(
      std::string_view(static_cast<const char*>(data->data()), data->size()));
}

}  // namespace cc
