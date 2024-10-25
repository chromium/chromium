// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_ORIGINS_H_
#define CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_ORIGINS_H_

#include <array>

namespace drive {

inline constexpr char kDriveFsNativeMessageHostName[] =
    "com.google.drive.nativeproxy";

inline constexpr auto kDriveFsNativeMessageHostOrigins =
    std::to_array({"chrome-extension://lmjegmlicamnimmfhcmpkclmigmmcbeh/"});

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_ORIGINS_H_
