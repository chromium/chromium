// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_ASH_H_
#define CHROME_BROWSER_ASH_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_ASH_H_

#include <memory>

namespace content {
class BrowserContext;
}

namespace extensions {
class NativeMessageHost;
}

namespace drive {

std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsNativeMessageHostAsh(content::BrowserContext* browser_context);

}  // namespace drive

#endif  //  CHROME_BROWSER_ASH_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_ASH_H_
