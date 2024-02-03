// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_SWITCHES_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_SWITCHES_H_

#include <string>
namespace extensions {

// Add "testrequest" to the update check request.
extern const char kSwitchTestRequestParam[];

// Forces a chrome channel during update requests.
extern const char kSwitchExtensionForceChannel[];

// Returns the chrome channel which should be used in queries for extension
// updates. It takes kSwitchExtensionForceChannel into account.
std::string GetChannelForExtensionUpdates();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_SWITCHES_H_
