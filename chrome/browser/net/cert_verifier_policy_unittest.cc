// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

class CertVerifierPoliciesTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// TODO(crbug.com/40928765): add tests for other fields as they are used by the
// Cert Management UI
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(CertVerifierPoliciesTest, ManagedPlatformIntegrationOn) {
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kCAPlatformIntegrationEnabled,
      std::make_unique<base::Value>(true));
  ProfileNetworkContextService service(profile_.get());

  ProfileNetworkContextService::CertificatePoliciesForView policies =
      service.GetCertificatePolicyForView();
  ASSERT_TRUE(policies.is_include_system_trust_store_managed);
  ASSERT_TRUE(policies.certificate_policies->include_system_trust_store);
}

TEST_F(CertVerifierPoliciesTest, ManagedPlatformIntegrationOff) {
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kCAPlatformIntegrationEnabled,
      std::make_unique<base::Value>(false));
  ProfileNetworkContextService service(profile_.get());

  ProfileNetworkContextService::CertificatePoliciesForView policies =
      service.GetCertificatePolicyForView();
  ASSERT_TRUE(policies.is_include_system_trust_store_managed);
  ASSERT_FALSE(policies.certificate_policies->include_system_trust_store);
}

TEST_F(CertVerifierPoliciesTest, UnmanagedPlatformIntegrationDefault) {
  ProfileNetworkContextService service(profile_.get());

  ProfileNetworkContextService::CertificatePoliciesForView policies =
      service.GetCertificatePolicyForView();
  ASSERT_FALSE(policies.is_include_system_trust_store_managed);
  ASSERT_TRUE(policies.certificate_policies->include_system_trust_store);
}

TEST_F(CertVerifierPoliciesTest, UnmanagedPlatformIntegrationOn) {
  profile_->GetTestingPrefService()->SetUserPref(
      prefs::kCAPlatformIntegrationEnabled,
      std::make_unique<base::Value>(true));
  ProfileNetworkContextService service(profile_.get());

  ProfileNetworkContextService::CertificatePoliciesForView policies =
      service.GetCertificatePolicyForView();
  ASSERT_FALSE(policies.is_include_system_trust_store_managed);
  ASSERT_TRUE(policies.certificate_policies->include_system_trust_store);
}

TEST_F(CertVerifierPoliciesTest, UnmanagedPlatformIntegrationOff) {
  profile_->GetTestingPrefService()->SetUserPref(
      prefs::kCAPlatformIntegrationEnabled,
      std::make_unique<base::Value>(false));
  ProfileNetworkContextService service(profile_.get());

  ProfileNetworkContextService::CertificatePoliciesForView policies =
      service.GetCertificatePolicyForView();
  ASSERT_FALSE(policies.is_include_system_trust_store_managed);
  ASSERT_FALSE(policies.certificate_policies->include_system_trust_store);
}

#endif

}  // namespace chrome_browser_net
