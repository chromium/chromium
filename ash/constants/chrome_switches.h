// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_CHROME_SWITCHES_H_
#define ASH_CONSTANTS_CHROME_SWITCHES_H_

// This file contains a few copy of web browser command line switch constants
// used in ChromeOS's OS system parts.
// The constants must have the same value with the ones defined in
// chrome/common/chrome_switches.h, and so should have static_assert() there.

namespace ash::chrome_switches {

// Sorted in the lexicographical order.
inline constexpr char kAppId[] = "app-id";
inline constexpr char kDisableDefaultApps[] = "disable-default-apps";
inline constexpr char kForceAppMode[] = "force-app-mode";
inline constexpr char kHideCrashRestoreBubble[] = "hide-crash-restore-bubble";
inline constexpr char kHomePage[] = "homepage";
inline constexpr char kIncognito[] = "incognito";
inline constexpr char kNoFirstRun[] = "no-first-run";
inline constexpr char kRestoreLastSession[] = "restore-last-session";
inline constexpr char kSilentLaunch[] = "silent-launch";
inline constexpr char kUserDataDir[] = "user-data-dir";
inline constexpr char kWebApkServerUrl[] = "webapk-server-url";

}  // namespace ash::chrome_switches

#endif  // ASH_CONSTANTS_CHROME_SWITCHES_H_
