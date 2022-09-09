// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include <memory>

namespace extensions {

class DisplayInfoProvider;

// Returns platform-specific DisplayInfoProvider instance.
std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
