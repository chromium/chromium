// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer for dial_internal_message_util.cc.

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"

namespace media_router {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Limit input size to prevent out-of-memory failures like the one seen in
  // crbug.com/964715.
  if (size > 16 * 1024)
    return 0;

  base::Optional<base::Value> input = base::JSONReader::Read(
      std::string(reinterpret_cast<const char*>(data), size));
  if (!input)
    return 0;

  std::string error_unused;
  auto dial_internal_message =
      DialInternalMessage::From(std::move(input.value()), &error_unused);
  if (!dial_internal_message)
    return 0;

  DialInternalMessageUtil::IsStopSessionMessage(*dial_internal_message);

  auto custom_dial_launch_message_body_unused =
      CustomDialLaunchMessageBody::From(*dial_internal_message);
  return 0;
}

}  // namespace media_router
