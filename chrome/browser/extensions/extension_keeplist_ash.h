// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_ASH_H_

#include <string>

namespace extensions {

// Returns true if the extension is kept to run in Ash. A small list of
// 1st party extensions will continue to run in Ash either since they are
// used to support Chrome OS features such as text to speech or vox,
// or they are not compatible with Lacros yet.
bool ExtensionRunsInAsh(const std::string& extension_id);

// Some extension apps will continue to run in Ash until they are either
// deprecated or migrated. This function returns whether a given app_id is on
// that keep list. This function must only be called from the UI thread.
bool ExtensionAppRunsInAsh(const std::string& app_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_ASH_H_
