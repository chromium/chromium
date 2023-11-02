// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_ISOLATION_ABOUT_FLAGS_H_
#define CHROME_BROWSER_SITE_ISOLATION_ABOUT_FLAGS_H_

#include <string>

#include "base/strings/strcat.h"

namespace about_flags {

constexpr char kSiteIsolationTrialOptOutInternalName[] =
    "site-isolation-trial-opt-out";

inline std::string SiteIsolationTrialOptOutChoiceEnabled() {
  return base::StrCat({kSiteIsolationTrialOptOutInternalName, "@1"});
}

}  // namespace about_flags

#endif  // CHROME_BROWSER_SITE_ISOLATION_ABOUT_FLAGS_H_
