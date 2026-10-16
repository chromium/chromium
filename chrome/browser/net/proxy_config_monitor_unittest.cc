// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/proxy_config_monitor.h"

#include <memory>
#include <optional>

#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ProxyConfigMonitorTest : public testing::Test {
 protected:
  void SetUp() override { profile_ = TestingProfile::Builder().Build(); }

  void SetManagedProxyPref(base::DictValue proxy_dict) {
    profile_->GetTestingPrefService()->SetManagedPref(
        proxy_config::prefs::kProxy,
        std::make_unique<base::Value>(std::move(proxy_dict)));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ProxyConfigMonitorTest, IsProxyConfigured_DirectReturnsFalse) {
  SetManagedProxyPref(ProxyConfigDictionary::CreateDirect());
  ProxyConfigMonitor monitor(profile_.get());

  EXPECT_EQ(monitor.IsProxyConfigured(), std::optional<bool>(false));
}

TEST_F(ProxyConfigMonitorTest, IsProxyConfigured_AutoDetectOnlyReturnsFalse) {
  // WPAD auto-detection (`auto_detect = true`) is enabled by default in Windows
  // OS settings even when no proxy exists on the network, so auto-detect alone
  // must not be treated as an explicit proxy configuration.
  SetManagedProxyPref(ProxyConfigDictionary::CreateAutoDetect());
  ProxyConfigMonitor monitor(profile_.get());

  EXPECT_EQ(monitor.IsProxyConfigured(), std::optional<bool>(false));
}

TEST_F(ProxyConfigMonitorTest, IsProxyConfigured_PacScriptReturnsTrue) {
  SetManagedProxyPref(ProxyConfigDictionary::CreatePacScript(
      "http://example.com/proxy.pac", /*pac_mandatory=*/false));
  ProxyConfigMonitor monitor(profile_.get());

  EXPECT_EQ(monitor.IsProxyConfigured(), std::optional<bool>(true));
}

TEST_F(ProxyConfigMonitorTest, IsProxyConfigured_FixedServersReturnsTrue) {
  SetManagedProxyPref(
      ProxyConfigDictionary::CreateFixedServers("http=127.0.0.1:8080", ""));
  ProxyConfigMonitor monitor(profile_.get());

  EXPECT_EQ(monitor.IsProxyConfigured(), std::optional<bool>(true));
}

TEST_F(ProxyConfigMonitorTest,
       IsProxyConfigured_ProxyOverrideRulesReturnsTrue) {
  SetManagedProxyPref(ProxyConfigDictionary::CreateDirect());
  auto rule = base::DictValue()
                  .Set("DestinationMatchers",
                       base::ListValue().Append("https://example.com"))
                  .Set("ProxyList",
                       base::ListValue().Append("HTTPS proxy.example.com:443"));
  auto rules_list = base::ListValue().Append(std::move(rule));
  profile_->GetTestingPrefService()->SetManagedPref(
      proxy_config::prefs::kProxyOverrideRules,
      std::make_unique<base::Value>(std::move(rules_list)));

  ProxyConfigMonitor monitor(profile_.get());

  EXPECT_EQ(monitor.IsProxyConfigured(), std::optional<bool>(true));
}

TEST_F(ProxyConfigMonitorTest, IsProxyConfigured_LocalStateConstructor) {
  TestingPrefServiceSimple local_state;
  PrefProxyConfigTrackerImpl::RegisterPrefs(local_state.registry());
  local_state.SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
          "http=127.0.0.1:8080", "")));

  ProxyConfigMonitor monitor(&local_state);

  EXPECT_EQ(monitor.IsProxyConfigured(), std::optional<bool>(true));
}

}  // namespace
