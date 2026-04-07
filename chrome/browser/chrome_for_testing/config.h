// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_FOR_TESTING_CONFIG_H_
#define CHROME_BROWSER_CHROME_FOR_TESTING_CONFIG_H_

class PrefService;

namespace chrome_for_testing {

// Loads Chrome for Testing prefs from the JSON file specified in the
// --chrome-for-testing-config command line switch. Returns false if the file is
// missing or contains invalid JSON.
bool LoadConfig(PrefService* pref_service);

// Convenience functions to check Chrome for Testing prefs returned by
// |g_browser_process->local_state()|.
bool IsEnableUserEducationUI();
bool IsEnableSearchEngineChoiceDialog();
bool IsEnableVirtualClipboard();

}  // namespace chrome_for_testing

#endif  // CHROME_BROWSER_CHROME_FOR_TESTING_CONFIG_H_
