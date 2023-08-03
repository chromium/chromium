// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_RULES_MANAGER_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_RULES_MANAGER_TEST_UTILS_H_

#include <string>

#include "base/values.h"

namespace policy::dlp_test_util {

// Data structure representing a DLP rule.
class DlpRule {
 public:
  DlpRule(const std::string& name,
          const std::string& description,
          const std::string& id);
  DlpRule();
  ~DlpRule();
  DlpRule(const DlpRule& other);
  DlpRule& AddSrcUrl(const std::string& url);
  DlpRule& AddDstUrl(const std::string& url);
  DlpRule& AddDstComponent(const std::string& component);
  DlpRule& AddRestriction(const std::string& type, const std::string& level);

  // Return a dictionary version of the rule that can be use to set up the
  // DataLeakPreventionRulesList policy.
  base::Value::Dict Create() const;

 private:
  const std::string name;
  const std::string description;
  const std::string id;
  std::vector<std::string> src_urls;
  std::vector<std::string> dst_urls;
  std::vector<std::string> dst_components;
  std::vector<std::pair<std::string, std::string>> restrictions;
};

}  // namespace policy::dlp_test_util

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_RULES_MANAGER_TEST_UTILS_H_
