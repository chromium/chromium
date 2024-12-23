// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_
#define CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_

namespace glic::prefs {

// ************* LOCAL STATE PREFS ***************
// These prefs are per-Chrome-installation

// Boolean pref that enables or disables the launcher.
inline constexpr char kGlicLauncherEnabled[] = "glic.launcher_enabled";

// Dictionary pref that keeps track of the registered hotkey for Glic.
inline constexpr char kGlicLauncherGlobalHotkey[] =
    "glic.launcher_global_hotkey";

// ************* PROFILE PREFS ***************
// Prefs below are tied to a user profile

// Boolean pref that enables or disables geolocation access for Glic.
inline constexpr char kGlicGeolocationEnabled[] = "glic.geolocation_enabled";
// Boolean pref that enables or disables microphone access for Glic.
inline constexpr char kGlicMicrophoneEnabled[] = "glic.microphone_enabled";
// Boolean pref that enables or disables tab context for Glic.
inline constexpr char kGlicTabContextEnabled[] = "glic.tab_context_enabled";

}  // namespace glic::prefs

#endif  // CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_
