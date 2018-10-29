// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace browser_switcher {
namespace prefs {

// Path to the executable of the alternative browser, or one of "${chrome}",
// "${ie}", "${firefox}", "${opera}", "${safari}".
const char kAlternativeBrowserPath[] =
    "browser_switcher.alternative_browser_path";

// Arguments to pass to the alternative browser when invoking it via
// |ShellExecute()|.
const char kAlternativeBrowserParameters[] =
    "browser_switcher.alternative_browser_parameters";

// List of host domain names to be opened in an alternative browser.
const char kUrlList[] = "browser_switcher.url_list";

// List of hosts that should not trigger a transition in either browser.
const char kUrlGreylist[] = "browser_switcher.url_greylist";

// If set to true, use the IE Enterprise Mode Sitelist policy.
const char kUseIeSitelist[] = "browser_switcher.use_ie_sitelist";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kAlternativeBrowserPath, "");
  registry->RegisterListPref(prefs::kAlternativeBrowserParameters);
  registry->RegisterListPref(prefs::kUrlList);
  registry->RegisterListPref(prefs::kUrlGreylist);
  registry->RegisterBooleanPref(prefs::kUseIeSitelist, false);
}

}  // namespace prefs
}  // namespace browser_switcher
