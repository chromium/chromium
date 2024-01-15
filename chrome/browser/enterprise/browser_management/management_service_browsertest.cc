// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kPrefName[] = "pref";

class TestPlatformManagementStatusProvider
    : public policy::ManagementStatusProvider {
 public:
  TestPlatformManagementStatusProvider(
      const std::string& cache_pref_name,
      policy::EnterpriseManagementAuthority authority)
      : policy::ManagementStatusProvider(cache_pref_name),
        authority_(authority) {}

  policy::EnterpriseManagementAuthority FetchAuthority() override {
    return authority_;
  }

  void UsePrefServiceAsCache(PrefService* prefs) override {
    static_cast<PrefRegistrySimple*>(prefs->DeprecatedGetPrefRegistry())
        ->RegisterIntegerPref(kPrefName, 0);
    if (prefs->HasPrefPath(kPrefName))
      cached_authority_ = static_cast<policy::EnterpriseManagementAuthority>(
          prefs->GetInteger(kPrefName));
    policy::ManagementStatusProvider::UsePrefServiceAsCache(prefs);
  }

  const std::optional<policy::EnterpriseManagementAuthority>& cached_authority()
      const {
    return cached_authority_;
  }

 private:
  std::optional<policy::EnterpriseManagementAuthority> cached_authority_;
  policy::EnterpriseManagementAuthority authority_;
};

}  // namespace

class ManagementServiceBrowserTest : public InProcessBrowserTest {
 public:
  ManagementServiceBrowserTest() = default;
  ~ManagementServiceBrowserTest() override = default;
  ManagementServiceBrowserTest(const ManagementServiceBrowserTest&) = delete;
  ManagementServiceBrowserTest& operator=(const ManagementServiceBrowserTest&) =
      delete;

 protected:
  // InProcessBrowserTest:
  void SetUp() override {
    auto status_providers =
        std::make_unique<TestPlatformManagementStatusProvider>(
            kPrefName, policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    platform_status_provider_ = status_providers.get();
    std::vector<std::unique_ptr<policy::ManagementStatusProvider>> providers;
    providers.emplace_back(std::move(status_providers));
    policy::ManagementServiceFactory::GetForPlatform()
        ->SetManagementStatusProviderForTesting(std::move(providers));
    InProcessBrowserTest::SetUp();
  }

 protected:
  raw_ptr<TestPlatformManagementStatusProvider> platform_status_provider_;
};

IN_PROC_BROWSER_TEST_F(ManagementServiceBrowserTest,
                       PRE_PlatformManagementServiceCache) {
  EXPECT_FALSE(platform_status_provider_->cached_authority().has_value());
}

IN_PROC_BROWSER_TEST_F(ManagementServiceBrowserTest,
                       PlatformManagementServiceCache) {
  EXPECT_EQ(policy::EnterpriseManagementAuthority::DOMAIN_LOCAL,
            *platform_status_provider_->cached_authority());
}
