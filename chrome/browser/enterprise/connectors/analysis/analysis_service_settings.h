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
#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/analysis_service_settings_base.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/url_matcher/url_matcher.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/browser/browser_context.h"
#endif

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {

#if BUILDFLAG(IS_CHROMEOS)
class SourceDestinationMatcherAsh;
#endif

// The settings for an analysis service obtained from a connector policy.
class AnalysisServiceSettings : public AnalysisServiceSettingsBase {
 public:
  explicit AnalysisServiceSettings(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config);
  AnalysisServiceSettings(const AnalysisServiceSettings&) = delete;
  AnalysisServiceSettings(AnalysisServiceSettings&&);
  AnalysisServiceSettings& operator=(const AnalysisServiceSettings&) = delete;
  AnalysisServiceSettings& operator=(AnalysisServiceSettings&&);
  ~AnalysisServiceSettings() override;

  // Get the settings to apply to a specific analysis. std::nullopt implies no
  // analysis should take place.
  std::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      DataRegion data_region) const;

#if BUILDFLAG(IS_CHROMEOS)
  std::optional<AnalysisSettings> GetAnalysisSettings(
      content::BrowserContext* context,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      DataRegion data_region) const;
#endif

  // TODO(crbug.com/444237640): Move getter methods to the base class.
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
  // Accessors for the pattern setting maps.
  static std::optional<URLPatternSettings> GetPatternSettings(
      const PatternSettings& patterns,
      base::MatcherStringPattern::ID match);

  // Helper methods for parsing the raw policy settings input
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  void ParseVerificationSignatures(const base::Value::Dict& settings_dict);
#endif

  // Returns the analysis settings with the specified tags.
  AnalysisSettings GetAnalysisSettingsWithTags(
      std::map<std::string, TagSettings> tags,
      DataRegion data_region) const;

  // Returns true if the settings were initialized correctly. If this returns
  // false, then GetAnalysisSettings will always return std::nullopt.
  bool IsValid() const;

#if BUILDFLAG(IS_CHROMEOS)
  // Updates the states of `source_destination_matcher_`,
  // `enabled_patterns_settings_` and/or `disabled_patterns_settings_` from a
  // policy value.
  void AddSourceDestinationSettings(
      const base::Value::Dict& source_destination_settings_value,
      bool enabled,
      base::MatcherStringPattern::ID* id) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Return tags found in |enabled_patterns_settings| corresponding to the
  // matches while excluding the ones in |disable_patterns_settings|.
  std::map<std::string, TagSettings> GetTags(
      const std::set<base::MatcherStringPattern::ID>& matches) const;

#if BUILDFLAG(IS_CHROMEOS)
  // A matcher to identify matching pairs of sources and destinations.
  // Set for ChromeOS' OnFileTransferEnterpriseConnector.
  std::unique_ptr<SourceDestinationMatcherAsh> source_destination_matcher_ =
      std::make_unique<SourceDestinationMatcherAsh>();
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Arrays of base64 encoded signing key signatures used to verify the
  // authenticity of the service provider.
  std::vector<std::string> verification_signatures_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_ANALYSIS_SERVICE_SETTINGS_H_
