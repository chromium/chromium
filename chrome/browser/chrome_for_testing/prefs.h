// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_FOR_TESTING_PREFS_H_
#define CHROME_BROWSER_CHROME_FOR_TESTING_PREFS_H_

class PrefRegistrySimple;
class PrefService;

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

// List of required components names and, optionally, their versions. Default is
// empty. Component names can be specified as patterns with * and ? wildcards.
inline constexpr char kRequiredComponents[] =
    "chrome_for_testing.required_components";

// Required components directory. Default is none, so components are installed
// in the user data directory.
inline constexpr char kRequiredComponentsDir[] =
    "chrome_for_testing.required_components_dir";

// Required components update timeout. Default is 15 seconds.
inline constexpr char kRequiredComponentsUpdateTimeout[] =
    "chrome_for_testing.required_components_update_timeout";

}  // namespace prefs

// Register Chrome for Testing prefs.
void RegisterPrefs(PrefRegistrySimple* registry);

// Reset Chrome for Testing prefs to default values.
void ClearPrefs(PrefService* pref_service);

}  // namespace chrome_for_testing

#endif  // CHROME_BROWSER_CHROME_FOR_TESTING_PREFS_H_
