// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SERVICE_SETTINGS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SERVICE_SETTINGS_H_

#include <set>
#include <string>

#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/url_matcher/url_matcher.h"

namespace enterprise_connectors {

// The settings for a report service obtained from a connector policy.
class FileSystemServiceSettings {
 public:
  explicit FileSystemServiceSettings(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config);
  FileSystemServiceSettings(FileSystemServiceSettings&&);
  ~FileSystemServiceSettings();

  // Get the settings to apply. base::nullopt implies no file system settings.
  base::Optional<FileSystemSettings> GetSettings(const GURL& url) const;

 private:
  // The setting to apply when a specific URL pattern is matched.
  struct URLPatternSettings {
    URLPatternSettings();
    URLPatternSettings(const URLPatternSettings&);
    URLPatternSettings(URLPatternSettings&&);
    URLPatternSettings& operator=(const URLPatternSettings&);
    URLPatternSettings& operator=(URLPatternSettings&&);
    ~URLPatternSettings();

    // Mime types that correspond to the pattern.
    std::set<std::string> mime_types;
  };

  // Map from an ID representing a specific matched pattern to its settings.
  using PatternSettings =
      std::map<url_matcher::URLMatcherConditionSet::ID, URLPatternSettings>;

  // Accessors for the pattern setting maps.
  static base::Optional<URLPatternSettings> GetPatternSettings(
      const PatternSettings& patterns,
      url_matcher::URLMatcherConditionSet::ID match);

  // Returns true if the settings were initialized correctly. If this returns
  // false, then GetSettings will always return base::nullopt.
  bool IsValid() const;

  // Updates the states of |matcher_|, |enabled_patterns_settings_| and/or
  // |disabled_patterns_settings_| from a policy value.
  void AddUrlPatternSettings(const base::Value& url_settings_value,
                             bool enabled,
                             url_matcher::URLMatcherConditionSet::ID* id);

  // Return mime types found in |enabled_patterns_settings| corresponding to the
  // matches while excluding the ones in |disable_patterns_settings|.
  std::set<std::string> GetMimeTypes(
      const std::set<url_matcher::URLMatcherConditionSet::ID>& matches) const;

  // The service provider name.
  std::string service_provider_name_;

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

  // When signing in to the service provider, only accounts that belong to this
  // enterprise are accepted.  This prevents people from connecting arbitrary
  // accounts and helps restrict to the enterprise the administrator wants.
  std::string enterprise_id_;

  // The enterprise domain name provided as a hint to the user when they are
  // are asked to sign-in to the service provider.  May be empty if not needed.
  std::string email_domain_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SERVICE_SETTINGS_H_
