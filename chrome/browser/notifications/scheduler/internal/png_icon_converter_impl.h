// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_PNG_ICON_CONVERTER_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_PNG_ICON_CONVERTER_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/notifications/scheduler/internal/icon_converter.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

class PngIconConverterImpl : public IconConverter {
 public:
  PngIconConverterImpl();
  ~PngIconConverterImpl() override;

 private:
  // IconConverter implementation.
  void ConvertIconToString(std::vector<SkBitmap> images,
                           EncodeCallback callback) override;
  void ConvertStringToIcon(std::vector<std::string> encoded_data,
                           DecodeCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(PngIconConverterImpl);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_PNG_ICON_CONVERTER_IMPL_H_
