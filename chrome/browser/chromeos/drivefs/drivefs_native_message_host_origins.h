// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_ORIGINS_H_
#define CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_ORIGINS_H_

#include "base/containers/span.h"

namespace drive {

extern const char kDriveFsNativeMessageHostName[];

extern const base::span<const char* const> kDriveFsNativeMessageHostOrigins;

}  // namespace drive

#endif  //  CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_ORIGINS_H_
