// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <iterator>

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "base/json/json_reader.h"

// Used to indicate an unset key/id/secret.  This works better with
// various unit tests than leaving the token empty.
#define DUMMY_API_TOKEN "dummytoken"

namespace enterprise_connectors {

namespace {

constexpr std::array<SupportedTag, 2> kGoogleDlpSupportedTags = {{
    {
        .name = "malware",
        .display_name = "Threat protection",
        .max_file_size = 52428800,
    },
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr AnalysisConfig kGoogleAnalysisConfig = {
    .url = "https://safebrowsing.google.com/safebrowsing/uploads/scan",
    .supported_tags = base::span<const SupportedTag>(kGoogleDlpSupportedTags),
};

constexpr std::array<SupportedTag, 1> kLocalTestSupportedTags = {{
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr std::array<SupportedTag, 1> kBrcmChrmCasSupportedTags = {{
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr AnalysisConfig kLocalTestUserAnalysisConfig = {
    .local_path = "path_user",
    .supported_tags = base::span<const SupportedTag>(kLocalTestSupportedTags),
    .user_specific = true,
};

constexpr AnalysisConfig kBrcmChrmCasAnalysisConfig = {
    .local_path = "brcm_chrm_cas",
    .supported_tags = base::span<const SupportedTag>(kBrcmChrmCasSupportedTags),
    .user_specific = false,
};

constexpr ReportingConfig kGoogleReportingConfig = {
    .url = "https://chromereporting-pa.googleapis.com/v1/events",
};

}  // namespace

const ServiceProviderConfig* GetServiceProviderConfig() {
  // The policy schema validates that the provider name is an expected value, so
  // when one is added to this dictionary it also needs to be added to the
  // corresponding policy definitions.
  // LINT.IfChange
  static constexpr ServiceProviderConfig kServiceProviderConfig =
      base::MakeFixedFlatMap<base::StringPiece, ServiceProvider>({
          {
              "google",
              {
                  .display_name = "Google Cloud",
                  .analysis = &kGoogleAnalysisConfig,
                  .reporting = &kGoogleReportingConfig,
              },
          },
          // TODO(b/226560946): Add the actual local content analysis service
          // providers to this config.
          {
              "local_user_agent",
              {
                  .display_name = "Test user agent",
                  .analysis = &kLocalTestUserAnalysisConfig,
              },
          },
          // Temporary code(b/268532118): Once DM server no longer sends
          // this value as a service_provider name, this block can be
          // removed.
          {
              "local_system_agent",
              {
                  .display_name = "Test system agent",
                  .analysis = &kBrcmChrmCasAnalysisConfig,
              },
          },
          {
              "brcm_chrm_cas",
              {
                  .display_name = "Broadcom Inc",
                  .analysis = &kBrcmChrmCasAnalysisConfig,
              },
          },
      });
  // LINT.ThenChange(//components/policy/resources/templates/policy_definitions/Miscellaneous)
  // The following policies should have their service_provider entries updated:
  //   //components/policy/resources/templates/policy_definitions/Miscellaneous/OnBulkDataEntryEnterpriseConnector.yaml,
  //   //components/policy/resources/templates/policy_definitions/Miscellaneous/OnFileAttachedEnterpriseConnector.yaml,
  //   //components/policy/resources/templates/policy_definitions/Miscellaneous/OnFileDownloadedEnterpriseConnector.yaml,
  //   //components/policy/resources/templates/policy_definitions/Miscellaneous/OnPrintEnterpriseConnector.yaml
  // )
  return &kServiceProviderConfig;
}

}  // namespace enterprise_connectors
