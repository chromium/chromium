// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include "chrome/browser/policy/policy_test_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace {

class ManagedBrowserUtilsBrowserTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  ManagedBrowserUtilsBrowserTest() = default;
  ~ManagedBrowserUtilsBrowserTest() override = default;

  bool managed_policy() { return GetParam(); }

  base::Value policy_value() {
    constexpr char kAutoSelectCertificateValue[] = R"({
      "pattern": "https://foo.com",
      "filter": {
        "ISSUER": {
          "O": "Chrome",
          "OU": "Chrome Org Unit",
          "CN": "Chrome Common Name"
        }
      }
    })";
    base::Value list(base::Value::Type::LIST);
    list.Append(kAutoSelectCertificateValue);
    return list;
  }
};

INSTANTIATE_TEST_SUITE_P(, ManagedBrowserUtilsBrowserTest, testing::Bool());

}  // namespace

IN_PROC_BROWSER_TEST_P(ManagedBrowserUtilsBrowserTest, LocalState) {
  EXPECT_FALSE(chrome::enterprise_util::IsMachinePolicyPref(
      prefs::kManagedAutoSelectCertificateForUrls));

  policy::PolicyMap policies;
  policies.Set(policy::key::kAutoSelectCertificateForUrls,
               managed_policy() ? policy::POLICY_LEVEL_MANDATORY
                                : policy::POLICY_LEVEL_RECOMMENDED,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
               policy_value(), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(managed_policy(), chrome::enterprise_util::IsMachinePolicyPref(
                                  prefs::kManagedAutoSelectCertificateForUrls));
}
