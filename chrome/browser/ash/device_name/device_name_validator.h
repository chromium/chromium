// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_VALIDATOR_H_
#define CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_VALIDATOR_H_

#include <string>

namespace ash {

// Valid names must be >0 characters and <=15 characters long and contain only
// letters, numbers or hyphens. Examples of invalid names include "Chrome
// OS" (uses a space), "ChromeOS!" (uses an exclamation point),
// "0123456789012345" (too long), "" (empty string).
bool IsValidDeviceName(const std::string& device_name);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_VALIDATOR_H_
