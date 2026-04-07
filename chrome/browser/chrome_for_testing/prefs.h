// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_FOR_TESTING_PREFS_H_
#define CHROME_BROWSER_CHROME_FOR_TESTING_PREFS_H_

class PrefRegistrySimple;

namespace chrome_for_testing {
namespace prefs {

// Show User Education UI. Default is false.
inline constexpr char kEnableUserEducationUI[] =
    "chrome_for_testing.enable_user_education_ui";

// Enable Search Engine Dialog. Default is false.
inline constexpr char kEnableSearchEngineChoiceDialog[] =
    "chrome_for_testing.enable_search_engine_choice_dialog";

// Enable process specific virtual clipboard. Default is false.
inline constexpr char kEnableVirtualClipboard[] =
    "chrome_for_testing.enable_virtual_clipboard";

}  // namespace prefs

// Register Chrome for Testing prefs.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace chrome_for_testing

#endif  // CHROME_BROWSER_CHROME_FOR_TESTING_PREFS_H_
