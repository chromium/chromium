// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_ENUMS_H_
#define CHROME_BROWSER_GLIC_GLIC_ENUMS_H_

namespace glic {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(InvocationSource)
enum class InvocationSource {
  kOsButton = 0,         // Button in the OS
  kOsButtonMenu = 1,     // Menu from button in the OS
  kOsHotkey = 2,         // OS-level hotkey
  kTopChromeButton = 3,  // Button in top-chrome
  kFre = 4,              // First run experience
  kProfilePicker = 5,    // From the profile picker
  kNudge = 6,            // From page actions
  kChroMenu = 7,         // From 3-dot menu.
  kMaxValue = kChroMenu,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicInvocationSource)

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_ENUMS_H_
