// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/device_name_validator.h"

#include "base/strings/string_util.h"

namespace ash {
namespace {

// For maximum compatibility with existing network services (e.g., Active
// Directory), the upper limit for hostname length is 15 characters.
const int kMaxDeviceNameLength = 15;

const char kDeviceNameAllowedChars[] = "0123456789-abcdefghijklmnopqrstuvwxyz";

}  // namespace

bool IsValidDeviceName(const std::string& device_name) {
  // Device name be not be empty string.
  if (device_name.empty())
    return false;

  // Device name should be <=15 characters long.
  if (device_name.length() > kMaxDeviceNameLength)
    return false;

  // Device name may contain only letters, numbers and hyphens.
  if (!base::ContainsOnlyChars(base::ToLowerASCII(device_name),
                               kDeviceNameAllowedChars)) {
    return false;
  }

  return true;
}

}  // namespace ash
