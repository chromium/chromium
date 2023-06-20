// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_NACL_DEPRECATION_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_NACL_DEPRECATION_H_

#include "base/feature_list.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

// Controls whether any NaCl plugins are allowed to be loaded.
// This can be overridden by enterprise policy. Exposed for testing.
BASE_DECLARE_FEATURE(kNaclAllow);

// This class exists to facilitate NaCl deprecation.
class ChromeBrowserMainExtraPartsNaclDeprecation
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecation() = default;

  // This method disables NaCl depending on field trial and policy settings. It
  // does so by setting a command line flag, which is the only way to get a
  // signal early enough to child processes. This is necessary since plugins are
  // loaded before field trial initialization.
  void PostEarlyInitialization() override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_NACL_DEPRECATION_H_
