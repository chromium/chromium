// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_COUNTRIES_IMPL_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_COUNTRIES_IMPL_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"

class PrivacySandboxCountriesImpl : public PrivacySandboxCountries {
 public:
  bool IsConsentCountry() override;

  bool IsRestOfWorldCountry() override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_COUNTRIES_IMPL_H_
