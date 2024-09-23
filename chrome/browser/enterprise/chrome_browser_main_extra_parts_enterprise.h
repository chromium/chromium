// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CHROME_BROWSER_MAIN_EXTRA_PARTS_ENTERPRISE_H_
#define CHROME_BROWSER_ENTERPRISE_CHROME_BROWSER_MAIN_EXTRA_PARTS_ENTERPRISE_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace enterprise_util {

// This class is used to run enterprise code at specific points in the
// chrome browser process life cycle.
class ChromeBrowserMainExtraPartsEnterprise
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsEnterprise();
  ChromeBrowserMainExtraPartsEnterprise(
      const ChromeBrowserMainExtraPartsEnterprise&) = delete;
  ChromeBrowserMainExtraPartsEnterprise& operator=(
      const ChromeBrowserMainExtraPartsEnterprise&) = delete;
  ~ChromeBrowserMainExtraPartsEnterprise() override;

  // ChromeBrowserMainExtraParts:
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
};

}  // namespace enterprise_util

#endif  // CHROME_BROWSER_ENTERPRISE_CHROME_BROWSER_MAIN_EXTRA_PARTS_ENTERPRISE_H_
