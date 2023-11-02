// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_

#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {

// Shows a notification about automatically granted access to a file system,
// i.e. the chrome.fileSystem.requestFileSystem() API.
void ShowNotificationForAutoGrantedRequestFileSystem(
    Profile* profile,
    const extensions::ExtensionId& extension_id,
    const std::string& extension_name,
    const std::string& volume_id,
    const std::string& volume_label,
    bool writable);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_
