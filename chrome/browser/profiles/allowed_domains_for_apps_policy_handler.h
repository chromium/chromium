// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_ALLOWED_DOMAINS_FOR_APPS_POLICY_HANDLER_H_
#define CHROME_BROWSER_PROFILES_ALLOWED_DOMAINS_FOR_APPS_POLICY_HANDLER_H_

#include "base/feature_list.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_migrator.h"

namespace policy {

BASE_DECLARE_FEATURE(kAllowedDomainsForAppsNewPolicyHandler);

bool UseAllowedDomainsForAppsNewPolicyHandler();

class AllowedDomainsForAppsPolicyHandler : public ListPolicyHandler {
 public:
  AllowedDomainsForAppsPolicyHandler();
  AllowedDomainsForAppsPolicyHandler(
      const AllowedDomainsForAppsPolicyHandler&) = delete;
  AllowedDomainsForAppsPolicyHandler& operator=(
      const AllowedDomainsForAppsPolicyHandler&) = delete;
  ~AllowedDomainsForAppsPolicyHandler() override;

  void ApplyList(base::Value::List filtered_list, PrefValueMap* prefs) override;
};

class AllowedDomainsForAppsPolicyMigrator : public PolicyMigrator {
 public:
  void Migrate(PolicyBundle* bundle) final;

 private:
  static void StringToList(base::Value* val);
};

}  // namespace policy

#endif  // CHROME_BROWSER_PROFILES_ALLOWED_DOMAINS_FOR_APPS_POLICY_HANDLER_H_
