// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_PREFS_H_
#define CHROME_BROWSER_TTC_CORE_PREFS_H_

class PrefRegistrySimple;

namespace ttc {

namespace prefs {

// Boolean. Whether the user has completed the TTC first run experience, i.e.
// accepted the FRE dialog. Until this is true, starting a session shows the
// FRE dialog.
inline constexpr char kTtcFreCompleted[] = "ttc.fre_completed";

}  // namespace prefs

// Registers TTC's profile prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_PREFS_H_
