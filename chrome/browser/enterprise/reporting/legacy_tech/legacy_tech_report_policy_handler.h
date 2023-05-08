// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_REPORT_POLICY_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_REPORT_POLICY_HANDLER_H_

#include "components/policy/core/browser/url_scheme_list_policy_handler.h"

namespace enterprise_reporting {

class LegacyTechReportPolicyHandler
    : public policy::URLSchemeListPolicyHandler {
 public:
  LegacyTechReportPolicyHandler();
  LegacyTechReportPolicyHandler(const LegacyTechReportPolicyHandler&) = delete;
  LegacyTechReportPolicyHandler& operator=(
      const LegacyTechReportPolicyHandler&) = delete;
  ~LegacyTechReportPolicyHandler() override;

  // policy::URLSchemeListPolicyHandler
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;

 protected:
  size_t max_items() override;
  bool ValidatePolicyEntry(const std::string* policy) override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_REPORT_POLICY_HANDLER_H_
