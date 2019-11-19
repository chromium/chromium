// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STARTUP_SETTINGS_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_STARTUP_SETTINGS_CACHE_H_

#include <string>

namespace chromeos {
namespace startup_settings_cache {

// On Chrome OS, the application locale is stored in local state prefs. The
// zygote needs the locale so it can load the correct resource bundle and
// provide localized strings to renderers. However, the zygote forks and engages
// the sandbox before the browser loads local state.
//
// Instead, cache the locale in a separate JSON file and read it on zygote
// startup. The additional disk read on startup is unfortunately, but it's only
// ~20 bytes and this approach performs better than other approaches (passing a
// resource bundle file descriptor to zygote on renderer fork, or pre-load the
// local state file on startup). On coral (dual core Celeron N3350 1.1 GHz) the
// file read takes < 2.5 ms and the write takes < 1 ms. https://crbug.com/510455
std::string ReadAppLocale();

// Writes the locale string to a JSON file on disk. See above.
void WriteAppLocale(std::string app_locale);

}  // namespace startup_settings_cache
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STARTUP_SETTINGS_CACHE_H_
