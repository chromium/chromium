// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace browser_switcher {
namespace prefs {

extern const char kAlternativeBrowserPath[];
extern const char kAlternativeBrowserParameters[];
extern const char kUrlList[];
extern const char kUrlGreylist[];
extern const char kUseIeSitelist[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace prefs
}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_
