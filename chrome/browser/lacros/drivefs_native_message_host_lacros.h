// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DRIVEFS_NATIVE_MESSAGE_HOST_LACROS_H_
#define CHROME_BROWSER_LACROS_DRIVEFS_NATIVE_MESSAGE_HOST_LACROS_H_

#include <memory>

namespace content {
class BrowserContext;
}

namespace extensions {
class NativeMessageHost;
}

namespace drive {

// Creates a native message host initiated by the Lacros browser that creates
// a connection to DriveFS via ash.
std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsNativeMessageHostLacros(content::BrowserContext* browser_context);

}  // namespace drive

#endif  //  CHROME_BROWSER_LACROS_DRIVEFS_NATIVE_MESSAGE_HOST_LACROS_H_
