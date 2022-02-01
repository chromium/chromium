// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_PREFS_H_

class PrefRegistrySimple;

namespace extensions {

namespace login_api {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace login_api

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_PREFS_H_
