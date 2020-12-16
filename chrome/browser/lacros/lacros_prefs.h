// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_PREFS_H_
#define CHROME_BROWSER_LACROS_LACROS_PREFS_H_

class PrefRegistrySimple;

namespace lacros_prefs {

// A preference for whether the "this is an experimental feature" banner has
// been shown to the user.
extern const char kShowedExperimentalBannerPref[];

// Local state prefs are also known as browser-wide prefs. This function
// registers Lacros-related local state prefs.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace lacros_prefs

#endif  // CHROME_BROWSER_LACROS_LACROS_PREFS_H_
