// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/enterprise/device_trust/core/device_trust_connector_service.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_signals {

namespace {

using device_signals::UserPermission;

}  // namespace

class UserPermissionServiceFactoryTest : public testing::Test {
 protected:
  UserPermissionServiceFactoryTest() {
    TestingProfile::Builder builder;
    builder.OverridePolicyConnectorIsManagedForTesting(true);
    profile_ = builder.Build();

    // `TestingProfile` doesn't create a `DeviceTrustConnectorService` by
    // default.
    enterprise_connectors::DeviceTrustConnectorServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile_.get(),
            base::BindLambdaForTesting([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  enterprise_connectors::DeviceTrustConnectorService>(
                  Profile::FromBrowserContext(context)->GetPrefs());
            }));
  }

  void SetAllowlistPolicy(const std::string& pref_name) {
    base::ListValue urls;
    urls.Append("https://www.example.com");
    profile_->GetTestingPrefService()->SetManagedPref(pref_name,
                                                      std::move(urls));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// Tests that an unaffiliated managed profile on a cloud-managed browser can
// collect signals without consent when only the browser-level policy needs
// them, but needs consent once the user-level policy needs them too.
TEST_F(UserPermissionServiceFactoryTest,
       UnaffiliatedProfile_OnlyBrowserPolicyNeedsSignals) {
  policy::ScopedManagementServiceOverrideForTesting management_override(
      policy::ManagementServiceFactory::GetForProfile(profile_.get()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
  device_signals::UserPermissionService* service =
      UserPermissionServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  SetAllowlistPolicy(
      enterprise_connectors::kBrowserContextAwareAccessSignalsAllowlistPref);
  EXPECT_EQ(service->CanCollectSignals(), UserPermission::kGranted);

  SetAllowlistPolicy(
      enterprise_connectors::kUserContextAwareAccessSignalsAllowlistPref);
  EXPECT_EQ(service->CanCollectSignals(), UserPermission::kMissingConsent);
}

}  // namespace enterprise_signals
