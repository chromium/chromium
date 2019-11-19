// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_CONVERTER_RESULT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_CONVERTER_RESULT_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

// Result from IconConverter's encoding process.
struct EncodeResult {
  EncodeResult(bool success, std::vector<std::string> data);
  bool operator==(const EncodeResult& other) const;
  ~EncodeResult();

  bool success;
  std::vector<std::string> encoded_data;

 private:
  DISALLOW_COPY_AND_ASSIGN(EncodeResult);
};

// Result from IconConverter's decoding process.
struct DecodeResult {
  DecodeResult(bool success, std::vector<SkBitmap> icons);
  bool operator==(const DecodeResult& other) const;
  ~DecodeResult();

  bool success;
  std::vector<SkBitmap> decoded_icons;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecodeResult);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_CONVERTER_RESULT_H_
