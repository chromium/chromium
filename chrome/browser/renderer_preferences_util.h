// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_PREFERENCES_UTIL_H_
#define CHROME_BROWSER_RENDERER_PREFERENCES_UTIL_H_

class Profile;

namespace blink {
struct RendererPreferences;
}  // namespace blink

namespace renderer_preferences_util {

// Copies system configuration preferences into |prefs|.
void UpdateFromSystemSettings(blink::RendererPreferences* prefs,
                              Profile* profile);

}  // namespace renderer_preferences_util

#endif  // CHROME_BROWSER_RENDERER_PREFERENCES_UTIL_H_
