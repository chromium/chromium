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
#include "chrome/browser/enterprise/connectors/analysis/source_destination_matcher_ash.h"
#include "content/public/browser/browser_context.h"
#endif

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {

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

  // This method extends the result of the base class's GetAnalysisSettings with
  // local analysis settings if applicable.
  std::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      DataRegion data_region) const override;

#if BUILDFLAG(IS_CHROMEOS)
  std::optional<AnalysisSettings> GetAnalysisSettings(
      content::BrowserContext* context,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      DataRegion data_region) const;
#endif

 private:
  LocalAnalysisSettings GetLocalAnalysisSettings() const;

  // Helper methods for parsing the raw policy settings input
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  void ParseVerificationSignatures(const base::Value::Dict& settings_dict);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  void ParseSourceDestinationPatternSettings(
      const base::Value::List* pattern_settings_list,
      bool is_enabled_pattern);

  // Updates the states of `source_destination_matcher_`,
  // `enabled_patterns_settings_` and/or `disabled_patterns_settings_` from a
  // policy value.
  void AddSourceDestinationSettings(
      const base::Value::Dict& source_destination_settings_value,
      bool enabled);

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
