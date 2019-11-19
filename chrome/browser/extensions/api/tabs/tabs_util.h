// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_UTIL_H_

// Use this file for platform specific code.  Declare any functions in here, and
// then define an empty implementation in tabs_util.cc, and put the actual code
// in eg. tabs_util_chromeos.cc.

class Browser;

namespace extensions {
namespace tabs_util {

// Set up the browser in the locked fullscreen state, and do any additional
// necessary adjustments.
void SetLockedFullscreenState(Browser* browser, bool locked);

}  // namespace tabs_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_UTIL_H_
