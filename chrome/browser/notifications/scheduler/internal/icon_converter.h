// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_CONVERTER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_CONVERTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/internal/icon_converter_result.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

// An interface class to provide functionalities to encode icons to strings
// and to decode strings to icons.
class IconConverter {
 public:
  using EncodeCallback =
      base::OnceCallback<void(std::unique_ptr<EncodeResult>)>;
  using DecodeCallback =
      base::OnceCallback<void(std::unique_ptr<DecodeResult>)>;

  // Converts SkBitmap icons to strings.
  virtual void ConvertIconToString(std::vector<SkBitmap> images,
                                   EncodeCallback callback) = 0;

  // Converts encoded strings to SkBitmap icons.
  virtual void ConvertStringToIcon(std::vector<std::string> encoded_data,
                                   DecodeCallback callback) = 0;

  virtual ~IconConverter() = default;

 protected:
  IconConverter() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(IconConverter);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_CONVERTER_H_
