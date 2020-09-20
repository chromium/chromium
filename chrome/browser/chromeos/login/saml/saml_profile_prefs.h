// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PROFILE_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PROFILE_PREFS_H_

class PrefRegistrySimple;

namespace chromeos {

// Registers all Saml-related profile prefs.
void RegisterSamlProfilePrefs(PrefRegistrySimple* registry);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PROFILE_PREFS_H_
