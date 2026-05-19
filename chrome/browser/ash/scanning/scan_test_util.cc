// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scan_test_util.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/strings/string_view_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace ash {

std::string CreateJpeg(const int alpha) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseARGB(alpha, 0, 0, 255);
  std::optional<std::vector<uint8_t>> bytes =
      gfx::JPEGCodec::Encode(bitmap, /*quality=*/90);
  CHECK(bytes);
  return std::string(base::as_string_view(bytes.value()));
}

}  // namespace ash
