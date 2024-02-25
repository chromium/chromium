// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PREFERS_DEFAULT_SCROLLBAR_STYLES_PREFS_H_
#define CHROME_BROWSER_ACCESSIBILITY_PREFERS_DEFAULT_SCROLLBAR_STYLES_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

void RegisterPrefersDefaultScrollbarStylesPrefs(
    user_prefs::PrefRegistrySyncable* registry);

#endif  // CHROME_BROWSER_ACCESSIBILITY_PREFERS_DEFAULT_SCROLLBAR_STYLES_PREFS_H_
