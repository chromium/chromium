// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_PREFS_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_PREFS_H_

class PrefRegistrySimple;

namespace enterprise_signin {

enum class ProfileReauthPrompt {
  kDoNotPrompt = 0,
  kPromptInTab = 1,
};

namespace prefs {
extern const char kProfileReauthPrompt[];
}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry);
}  // namespace enterprise_signin

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_PREFS_H_
