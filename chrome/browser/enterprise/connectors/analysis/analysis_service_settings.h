// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_ANALYSIS_SERVICE_SETTINGS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_ANALYSIS_SERVICE_SETTINGS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/url_matcher/url_matcher.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/public/browser/browser_context.h"
#endif

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class SourceDestinationMatcherAsh;
#endif

// The settings for an analysis service obtained from a connector policy.
class AnalysisServiceSettings {
 public:
  explicit AnalysisServiceSettings(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config);
  AnalysisServiceSettings(AnalysisServiceSettings&&);
  ~AnalysisServiceSettings();

  // Get the settings to apply to a specific analysis. std::nullopt implies no
  // analysis should take place.
  std::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      DataRegion data_region) const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<AnalysisSettings> GetAnalysisSettings(
      content::BrowserContext* context,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      DataRegion data_region) const;
#endif

  // Get the block_until_verdict setting if the settings are valid.
  bool ShouldBlockUntilVerdict() const;

  // Get the default_action setting if the settings are valid.
  bool ShouldBlockByDefault() const;

  // Get the custom message/learn more URL. Returns std::nullopt if the
  // settings are invalid or if the message/URL are empty.
  std::optional<std::u16string> GetCustomMessage(const std::string& tag);
  std::optional<GURL> GetLearnMoreUrl(const std::string& tag);
  bool GetBypassJustificationRequired(const std::string& tag);

  std::string service_provider_name() const { return service_provider_name_; }

  // Helpers for convenient check of the underlying variant.
  bool is_cloud_analysis() const;
  bool is_local_analysis() const;

  const AnalysisConfig* GetAnalysisConfig() const { return analysis_config_; }

 private:
  // The setting to apply when a specific URL pattern is matched.
  struct URLPatternSettings {
    URLPatternSettings();
    URLPatternSettings(const URLPatternSettings&);
    URLPatternSettings(URLPatternSettings&&);
    URLPatternSettings& operator=(const URLPatternSettings&);
    URLPatternSettings& operator=(URLPatternSettings&&);
    ~URLPatternSettings();

    // Tags that correspond to the pattern.
    std::set<std::string> tags;
  };

  // Map from an ID representing a specific matched pattern to its settings.
  using PatternSettings =
      std::map<base::MatcherStringPattern::ID, URLPatternSettings>;

  // Accessors for the pattern setting maps.
  static std::optional<URLPatternSettings> GetPatternSettings(
      const PatternSettings& patterns,
      base::MatcherStringPattern::ID match);

  // Returns the analysis settings with the specified tags.
  AnalysisSettings GetAnalysisSettingsWithTags(
      std::map<std::string, TagSettings> tags,
      DataRegion data_region) const;

  // Returns true if the settings were initialized correctly. If this returns
  // false, then GetAnalysisSettings will always return std::nullopt.
  bool IsValid() const;

  // Updates the states of `matcher_`, `enabled_patterns_settings_` and/or
  // `disabled_patterns_settings_` from a policy value.
  void AddUrlPatternSettings(const base::Value::Dict& url_settings_dict,
                             bool enabled,
                             base::MatcherStringPattern::ID* id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Updates the states of `source_destination_matcher_`,
  // `enabled_patterns_settings_` and/or `disabled_patterns_settings_` from a
  // policy value.
  void AddSourceDestinationSettings(
      const base::Value::Dict& source_destination_settings_value,
      bool enabled,
      base::MatcherStringPattern::ID* id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Return tags found in |enabled_patterns_settings| corresponding to the
  // matches while excluding the ones in |disable_patterns_settings|.
  std::map<std::string, TagSettings> GetTags(
      const std::set<base::MatcherStringPattern::ID>& matches) const;

  // The service provider matching the name given in a Connector policy. nullptr
  // implies that a corresponding service provider doesn't exist and that these
  // settings are not valid.
  raw_ptr<const AnalysisConfig> analysis_config_ = nullptr;

  // The URL matcher created from the patterns set in the analysis policy. The
  // condition set IDs returned after matching against a URL can be used to
  // check |enabled_patterns_settings| and |disable_patterns_settings| to
  // obtain URL-specific settings.
  std::unique_ptr<url_matcher::URLMatcher> matcher_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // A matcher to identify matching pairs of sources and destinations.
  // Set for ChromeOS' OnFileTransferEnterpriseConnector.
  std::unique_ptr<SourceDestinationMatcherAsh> source_destination_matcher_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  BlockUntilVerdict block_until_verdict_ = BlockUntilVerdict::kNoBlock;
  DefaultAction default_action_ = DefaultAction::kAllow;
  bool block_password_protected_files_ = false;
  bool block_large_files_ = false;
  size_t minimum_data_size_ = 100;
  // A map from tag (dlp, malware, etc) to the custom message, "learn more" link
  // and other settings associated to a specific tag.
  std::map<std::string, TagSettings> tags_;
  std::string service_provider_name_;

  // Arrays of base64 encoded signing key signatures used to verify the
  // authenticity of the service provider.
  std::vector<std::string> verification_signatures_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_ANALYSIS_SERVICE_SETTINGS_H_
