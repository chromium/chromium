// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_H_

#include <memory>

#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace drive {

extern const char kDriveFsNativeMessageHostName[];

extern const char* const kDriveFsNativeMessageHostOrigins[];

extern const size_t kDriveFsNativeMessageHostOriginsSize;

std::unique_ptr<extensions::NativeMessageHost> CreateDriveFsNativeMessageHost(
    content::BrowserContext* browser_context);

}  // namespace drive

#endif  //  CHROME_BROWSER_CHROMEOS_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_H_
