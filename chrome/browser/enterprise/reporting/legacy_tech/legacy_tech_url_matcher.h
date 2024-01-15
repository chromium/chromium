// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_URL_MATCHER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_URL_MATCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/url_matcher/url_matcher.h"

class GURL;
class Profile;

namespace enterprise_reporting {

class LegacyTechURLMatcher {
 public:
  explicit LegacyTechURLMatcher(Profile* profile);
  LegacyTechURLMatcher(const LegacyTechURLMatcher&) = delete;
  LegacyTechURLMatcher& operator=(const LegacyTechURLMatcher&) = delete;
  ~LegacyTechURLMatcher();

  void OnPrefUpdated();

  std::optional<std::string> GetMatchedURL(const GURL& url) const;

 private:
  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_;

  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
  std::vector<size_t> path_length_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_URL_MATCHER_H_
