// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/icon_converter_result.h"

#include <utility>
#include <vector>

namespace notifications {

EncodeResult::EncodeResult(bool success, std::vector<std::string> data)
    : success(success), encoded_data(std::move(data)) {}

bool EncodeResult::operator==(const EncodeResult& other) const {
  return success == other.success &&
         encoded_data.size() == other.encoded_data.size();
}

EncodeResult::~EncodeResult() = default;

DecodeResult::DecodeResult(bool success, std::vector<SkBitmap> icons)
    : success(success), decoded_icons(std::move(icons)) {}

bool DecodeResult::operator==(const DecodeResult& other) const {
  return success == other.success &&
         decoded_icons.size() == other.decoded_icons.size();
}

DecodeResult::~DecodeResult() = default;

}  // namespace notifications
