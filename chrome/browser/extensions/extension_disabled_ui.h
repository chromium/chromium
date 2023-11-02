// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_UI_H_

namespace extensions {

class Extension;
class ExtensionService;

// Adds a global error to inform the user that an extension was
// disabled after upgrading to higher permissions.
// If |is_remote_install| is true, the extension was disabled because
// it was installed remotely.
void AddExtensionDisabledError(ExtensionService* service,
                               const Extension* extension,
                               bool is_remote_install);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_UI_H_
