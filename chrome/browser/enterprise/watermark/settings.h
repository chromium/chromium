// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_SETTINGS_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_SETTINGS_H_
#include "third_party/skia/include/core/SkColor.h"

namespace enterprise_watermark {

SkColor GetFillColor();
SkColor GetOutlineColor();

}  // namespace enterprise_watermark

#endif  // CHROME_BROWSER_ENTERPRISE_WATERMARK_SETTINGS_H_
