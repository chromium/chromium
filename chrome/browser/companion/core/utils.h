// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_UTILS_H_
#define CHROME_BROWSER_COMPANION_CORE_UTILS_H_

#include <string>

#include "url/gurl.h"

namespace companion {

std::string GetHomepageURLForCompanion();
std::string GetImageUploadURLForCompanion();
bool GetShouldIssuePreconnectForCompanion();
std::string GetPreconnectKeyForCompanion();
bool GetShouldIssueProcessPrewarmingForCompanion();
bool ShouldEnableOpenCompanionForImageSearch();
bool ShouldEnableOpenCompanionForWebSearch();
bool ShouldOpenLinksInCurrentTab();
std::string GetExpsRegistrationSuccessPageURLs();
std::string GetCompanionIPHBlocklistedPageURLs();
bool IsValidPageURLForCompanion(const GURL& url);
bool IsSafeURLFromCompanion(const GURL& url);
bool ShouldOpenContextualLensPanel();

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_UTILS_H_
