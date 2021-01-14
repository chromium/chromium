// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SERVICE_SETTINGS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SERVICE_SETTINGS_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/url_matcher/url_matcher.h"

namespace enterprise_connectors {

// The settings for an analysis service obtained from a connector policy.
class AnalysisServiceSettings {
 public:
  explicit AnalysisServiceSettings(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config);
  AnalysisServiceSettings(AnalysisServiceSettings&&);
  ~AnalysisServiceSettings();

  // Get the settings to apply to a specific analysis. base::nullopt implies no
  // analysis should take place.
  base::Optional<AnalysisSettings> GetAnalysisSettings(const GURL& url) const;

  // Get the block_until_verdict setting if the settings are valid.
  bool ShouldBlockUntilVerdict() const;

 private:
  // The setting to apply when a specific URL pattern is matched.
  struct URLPatternSettings {
    URLPatternSettings();
    URLPatternSettings(const URLPatternSettings&);
    ~URLPatternSettings();

    // Tags that correspond to the pattern.
    std::set<std::string> tags;
  };

  // Map from an ID representing a specific matched pattern to its settings.
  using PatternSettings =
      std::map<url_matcher::URLMatcherConditionSet::ID, URLPatternSettings>;

  // Accessors for the pattern setting maps.
  static base::Optional<URLPatternSettings> GetPatternSettings(
      const PatternSettings& patterns,
      url_matcher::URLMatcherConditionSet::ID match);

  // Returns true if the settings were initialized correctly. If this returns
  // false, then GetAnalysisSettings will always return base::nullopt.
  bool IsValid() const;

  // Updates the states of |matcher_|, |enabled_patterns_settings_| and/or
  // |disabled_patterns_settings_| from a policy value.
  void AddUrlPatternSettings(const base::Value& url_settings_value,
                             bool enabled,
                             url_matcher::URLMatcherConditionSet::ID* id);

  // Return tags found in |enabled_patterns_settings| corresponding to the
  // matches while excluding the ones in |disable_patterns_settings|.
  std::set<std::string> GetTags(
      const std::set<url_matcher::URLMatcherConditionSet::ID>& matches) const;

  // The service provider matching the name given in a Connector policy. nullptr
  // implies that a corresponding service provider doesn't exist and that these
  // settings are not valid.
  const ServiceProviderConfig::ServiceProvider* service_provider_ = nullptr;

  // The URL matcher created from the patterns set in the analysis policy. The
  // condition set IDs returned after matching against a URL can be used to
  // check |enabled_patterns_settings| and |disable_patterns_settings| to
  // obtain URL-specific settings.
  std::unique_ptr<url_matcher::URLMatcher> matcher_;

  // These members map URL patterns to corresponding settings.  If an entry in
  // the "enabled" or "disabled" lists contains more than one pattern in its
  // "url_list" property, only the last pattern's matcher ID will be added the
  // map.  This keeps the count of these maps smaller and keeps the code from
  // duplicating memory for the settings, which are the same for all URL
  // patterns in a given entry. This optimization works by using
  // std::map::upper_bound to access these maps. The IDs in the disabled
  // settings must be greater than the ones in the enabled settings for this to
  // work and avoid having the two maps cover an overlap of matches.
  PatternSettings enabled_patterns_settings_;
  PatternSettings disabled_patterns_settings_;

  BlockUntilVerdict block_until_verdict_ = BlockUntilVerdict::NO_BLOCK;
  bool block_password_protected_files_ = false;
  bool block_large_files_ = false;
  bool block_unsupported_file_types_ = false;
  size_t minimum_data_size_ = 100;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SERVICE_SETTINGS_H_
