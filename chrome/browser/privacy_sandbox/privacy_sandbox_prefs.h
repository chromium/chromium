// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_

#include "components/prefs/pref_registry_simple.h"

namespace prefs {

// Boolean that is true when Privacy Sandbox APIs are enabled. If the
// PrivacySandboxSettings feature is enabled, this Boolean is controlled by the
// associated UI; if it is disabled, it is controlled by third party cookie
// blocking settings.
extern const char kPrivacySandboxApisEnabled[];

// Boolean that indicates if a user has manually toggled the settings associated
// with the PrivacySandboxSettings feature.
extern const char kPrivacySandboxManauallyControlled[];

}  // namespace prefs

namespace privacy_sandbox {

// Registers user preferences related to privacy sandbox.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
