// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_CHROMEOS_H_

#include <string>

namespace extensions {

// By default most extensions will not work properly if they run in both ash and
// lacros. This is the list of exceptions.
bool ExtensionRunsInBothOSAndStandaloneBrowser(const std::string& extension_id);

// By default most extension apps will not work properly if they run in both
// ash and lacros. This is the list of exceptions.
bool ExtensionAppRunsInBothOSAndStandaloneBrowser(
    const std::string& extension_id);

// Returns true if the extension is kept to run in Ash. A small list of 1st
// party extensions will continue to run in Ash either since they are used to
// support Chrome OS features such as text to speech or vox, or they are not
// compatible with Lacros yet. When this method is invoked in Lacros, it may not
// know about OS-specific extensions that are compiled into ash.
bool ExtensionRunsInOS(const std::string& extension_id);

// Some extension apps will continue to run in Ash until they are either
// deprecated or migrated. This function returns whether a given app_id is on
// that keep list. This function must only be called from the UI thread. When
// this method is invoked in Lacros, it may not know about OS-specific
// extensions that are compiled into ash.
bool ExtensionAppRunsInOS(const std::string& app_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_CHROMEOS_H_
