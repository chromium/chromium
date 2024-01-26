// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace {

// Verify that all the proxy prefs are set to the specified expected values.
void VerifyProxyPrefs(PrefService* prefs,
                      const std::string& expected_proxy_server,
                      const std::string& expected_proxy_pac_url,
                      std::optional<bool> expected_proxy_pac_mandatory,
                      const std::string& expected_proxy_bypass_list,
                      const ProxyPrefs::ProxyMode& expected_proxy_mode) {
  const base::Value::Dict& pref_dict =
      prefs->GetDict(proxy_config::prefs::kProxy);
  ProxyConfigDictionary dict(pref_dict.Clone());
  std::string s;
  bool b;
  if (expected_proxy_server.empty()) {
    EXPECT_FALSE(dict.GetProxyServer(&s));
  } else {
    ASSERT_TRUE(dict.GetProxyServer(&s));
    EXPECT_EQ(expected_proxy_server, s);
  }
  if (expected_proxy_pac_url.empty()) {
    EXPECT_FALSE(dict.GetPacUrl(&s));
  } else {
    ASSERT_TRUE(dict.GetPacUrl(&s));
    EXPECT_EQ(expected_proxy_pac_url, s);
  }
  if (!expected_proxy_pac_mandatory) {
    EXPECT_FALSE(dict.GetPacMandatory(&b));
  } else {
    ASSERT_TRUE(dict.GetPacMandatory(&b));
    EXPECT_EQ(*expected_proxy_pac_mandatory, b);
  }
  if (expected_proxy_bypass_list.empty()) {
    EXPECT_FALSE(dict.GetBypassList(&s));
  } else {
    ASSERT_TRUE(dict.GetBypassList(&s));
    EXPECT_EQ(expected_proxy_bypass_list, s);
  }
  ProxyPrefs::ProxyMode mode;
  ASSERT_TRUE(dict.GetMode(&mode));
  EXPECT_EQ(expected_proxy_mode, mode);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyTest, SeparateProxyPoliciesMerging) {
  // Add an individual proxy policy value.
  PolicyMap policies;
  policies.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(3), nullptr);
  UpdateProviderPolicy(policies);

  VerifyProxyPrefs(g_browser_process->local_state(), std::string(),
                   std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_SYSTEM);
  VerifyProxyPrefs(chrome_test_utils::GetProfile(this)->GetPrefs(),
                   std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_SYSTEM);
}

}  // namespace policy
