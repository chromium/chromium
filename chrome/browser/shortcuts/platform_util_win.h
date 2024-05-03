// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_PLATFORM_UTIL_WIN_H_
#define CHROME_BROWSER_SHORTCUTS_PLATFORM_UTIL_WIN_H_

namespace base {
class FilePath;
}  // namespace base

namespace shortcuts {

// Path of chrome_proxy.exe, whose shortcuts are used to launch PWAs and open
// non-app urls.
base::FilePath GetChromeProxyPath();

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_PLATFORM_UTIL_WIN_H_
