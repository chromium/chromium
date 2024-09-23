// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This namespace provides various helpers and state relating to Google Chrome
// distributions (such as RLZ), and specifically relating to the brand of the
// current install. Brands are codes that are assigned to partners for tracking
// distribution information.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_H__
#define CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_H__

#include <string>

namespace google_brand {

extern const char* g_brand_for_testing;

// Returns in |brand| the brand code or distribution tag that has been
// assigned to a partner. Returns false if the information is not available.
// TODO(asvitkine): These APIs should return std::optional<std::string>.
bool GetBrand(std::string* brand);

// Returns in |brand| the reactivation brand code or distribution tag
// that has been assigned to a partner for reactivating a dormant chrome
// install. Returns false if the information is not available.
bool GetReactivationBrand(std::string* brand);

// The same as GetBrand() on non-ChromeOS platforms. On ChromeOS, returns a
// variation of the brand code based on enrollment type.
// TODO(crbug.com/40595214): Rename this to GetBrand and replace the current
// one.
bool GetRlzBrand(std::string* brand);

// True if a build is strictly organic, according to its brand code.
bool IsOrganic(const std::string& brand);

// True if a build should run as organic during first run. This uses
// a slightly different set of brand codes from the standard IsOrganic
// method.
bool IsOrganicFirstRun(const std::string& brand);

// True if |brand| is an internet cafe brand code.
bool IsInternetCafeBrandCode(const std::string& brand);

// True if |brand| is an enterprise brand code.
bool IsEnterprise(const std::string& brand);

// This class is meant to be used only from test code, and sets the brand
// code returned by the function GetBrand() above while the object exists.
class BrandForTesting {
 public:
  explicit BrandForTesting(const std::string& brand);

  BrandForTesting(const BrandForTesting&) = delete;
  BrandForTesting& operator=(const BrandForTesting&) = delete;

  ~BrandForTesting();

 private:
  std::string brand_;
};

}  // namespace google_brand

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_H__
