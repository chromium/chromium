// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_

#include "base/memory/weak_ptr.h"

class Profile;

namespace file_manager {
class Volume;
}  // namespace file_manager

namespace extensions {
class Extension;

// Shows a notification about automatically granted access to a file system,
// i.e. the chrome.fileSystem.requestFileSystem() API.
void ShowNotificationForAutoGrantedRequestFileSystem(
    Profile* profile,
    const extensions::Extension& extension,
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_
