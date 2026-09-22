// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string>

#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/android/chrome_jni_headers/QRCodeGenerator_jni.h"

static SkBitmap JNI_QRCodeGenerator_GenerateBitmap(
    const std::string& url_string) {
  // TODO(https://crbug.com/325664342): Audit if `QuietZone::kIncluded`
  // can/should be used instead (this may require testing if the different image
  // size works well with surrounding UI elements).  Note that the absence of a
  // quiet zone may interfere with decoding of QR codes even for small codes
  // (for examples see #comment8, #comment9 and #comment6 in the bug).
  auto qr_image = qr_code_generator::GenerateBitmap(
      base::as_byte_span(url_string), qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kDino,
      qr_code_generator::QuietZone::kWillBeAddedByClient);

  return qr_image.value_or(SkBitmap());
}

DEFINE_JNI(QRCodeGenerator)
