// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_PREFS_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_PREFS_H_

class PrefRegistrySimple;

namespace kaleidoscope {
namespace prefs {

// Stores the latest version of the first run experience that was completed
// successfully by the user.
extern const char kKaleidoscopeFirstRunCompleted[];

// Stores true if the user has consented to us selecting Media Feeds for display
// automatically.
extern const char kKaleidoscopeAutoSelectMediaFeeds[];

// Stores true if Kaleidoscope has been enabled/disabled by an administrator.
extern const char kKaleidoscopePolicyEnabled[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace kaleidoscope

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_PREFS_H_
