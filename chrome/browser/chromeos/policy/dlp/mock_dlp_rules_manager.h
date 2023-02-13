// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_RULES_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_RULES_MANAGER_H_

#include <utility>

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {
class MockDlpRulesManager : public DlpRulesManager {
 public:
  MockDlpRulesManager();
  ~MockDlpRulesManager() override;

  MOCK_CONST_METHOD2(IsRestricted,
                     Level(const GURL& source, Restriction restriction));

  MOCK_CONST_METHOD4(IsRestrictedByAnyRule,
                     Level(const GURL& source,
                           Restriction restriction,
                           std::string* out_source_pattern,
                           RuleMetadata* out_rule_metadata));

  MOCK_CONST_METHOD6(IsRestrictedDestination,
                     Level(const GURL& source,
                           const GURL& destination,
                           Restriction restriction,
                           std::string* out_source_pattern,
                           std::string* out_destination_pattern,
                           RuleMetadata* out_rule_metadata));

  MOCK_CONST_METHOD5(IsRestrictedComponent,
                     Level(const GURL& source,
                           const Component& destination,
                           Restriction restriction,
                           std::string* out_source_pattern,
                           RuleMetadata* out_rule_metadata));

  MOCK_METHOD(AggregatedDestinations,
              GetAggregatedDestinations,
              (const GURL& source, Restriction restriction),
              (override, const));

  MOCK_METHOD(AggregatedComponents,
              GetAggregatedComponents,
              (const GURL& source, Restriction restriction),
              (override, const));

  MOCK_CONST_METHOD0(IsReportingEnabled, bool());

  MOCK_CONST_METHOD0(GetReportingManager, DlpReportingManager*());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_CONST_METHOD0(GetDlpFilesController, DlpFilesController*());
#endif

  MOCK_CONST_METHOD4(GetSourceUrlPattern,
                     std::string(const GURL& source_url,
                                 Restriction restriction,
                                 Level level,
                                 RuleMetadata* out_rule_metadata));

  MOCK_CONST_METHOD0(GetClipboardCheckSizeLimitInBytes, size_t());

  MOCK_METHOD(bool, IsFilesPolicyEnabled, (), (override, const));

  MOCK_CONST_METHOD2(
      GetDisallowedFileTransfers,
      std::vector<uint64_t>(const std::vector<FileMetadata>& files_entries,
                            const GURL& destination));
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_RULES_MANAGER_H_
