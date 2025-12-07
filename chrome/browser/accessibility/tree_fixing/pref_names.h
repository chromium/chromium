// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_PREF_NAMES_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_PREF_NAMES_H_

class Profile;

namespace prefs {

inline constexpr char kAccessibilityAXTreeFixingEnabled[] =
    "settings.a11y.enable_ax_tree_fixing";

}  // namespace prefs

namespace tree_fixing {

// To avoid using any server-side tree fixing service, it is disabled in
// Incognito profiles.
void InitOffTheRecordPrefs(Profile* off_the_record_profile);

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_PREF_NAMES_H_
