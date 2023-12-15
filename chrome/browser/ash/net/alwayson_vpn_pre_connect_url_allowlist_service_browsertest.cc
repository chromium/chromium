// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/alwayson_vpn_pre_connect_url_allowlist_service.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/net/alwayson_vpn_pre_connect_url_allowlist_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {
// The network paths for the default services created by the
// FakeShillManagerClient.
constexpr char kDefaultEthernetPath[] = "/service/eth1";
constexpr char kDefaultVpnPath[] = "/service/vpn1";

constexpr char kFakePrimaryUsername[] = "test-primary@example.com";

constexpr char kTestUrl[] = "test.url.com";
}  // namespace

namespace ash {

class AlwaysOnVpnPreConnectUrlAllowlistServiceTest
    : public InProcessBrowserTest {
 public:
  AlwaysOnVpnPreConnectUrlAllowlistServiceTest() {}
  AlwaysOnVpnPreConnectUrlAllowlistServiceTest(
      const AlwaysOnVpnPreConnectUrlAllowlistServiceTest&) = delete;
  AlwaysOnVpnPreConnectUrlAllowlistServiceTest& operator=(
      const AlwaysOnVpnPreConnectUrlAllowlistServiceTest&) = delete;
  ~AlwaysOnVpnPreConnectUrlAllowlistServiceTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ash::AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetInstance()
        ->SetServiceIsNULLWhileTestingForTesting(
            /*service_is_null_while_testing=*/false);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    ash::AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetInstance()
        ->SetServiceIsNULLWhileTestingForTesting(
            /*service_is_null_while_testing=*/true);
  }
};

IN_PROC_BROWSER_TEST_F(AlwaysOnVpnPreConnectUrlAllowlistServiceTest,
                       NoServiceForUnmanagedProfiles) {
  // The service should be null because the profile is not managed.
  EXPECT_FALSE(
      ash::AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetForProfile(
          browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(AlwaysOnVpnPreConnectUrlAllowlistServiceTest,
                       NoServiceForSecondaryProfiles) {
  // Create a secondary managed profile.
  TestingProfile::Builder profile_builder;
  profile_builder.OverridePolicyConnectorIsManagedForTesting(
      /*is_managed*/ true);
  std::unique_ptr<TestingProfile> managed_profile = profile_builder.Build();

  EXPECT_FALSE(
      ash::AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetForProfile(
          browser()->profile()));
}

class AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest
    : public AlwaysOnVpnPreConnectUrlAllowlistServiceTest {
 public:
  AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest() {}
  AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest(
      const AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest&) =
      delete;
  AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest& operator=(
      const AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest&) =
      delete;
  ~AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest() override =
      default;

  void SetUpOnMainThread() override {
    AlwaysOnVpnPreConnectUrlAllowlistServiceTest::SetUpOnMainThread();
    network_handler_test_helper_ =
        std::make_unique<::ash::NetworkHandlerTestHelper>();
    ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
        "/service/wifi1", shill::kStateProperty,
        base::Value(shill::kStateIdle));

    // Create a primary managed profile.
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII(
        BrowserContextHelper::GetUserBrowserContextDirName(
            user_manager::FakeUserManager::GetFakeUsernameHash(
                AccountId::FromUserEmail(kFakePrimaryUsername)))));
    profile_builder.SetProfileName(kFakePrimaryUsername);
    profile_builder.OverridePolicyConnectorIsManagedForTesting(
        /*is_managed*/ true);

    managed_profile_ = profile_builder.Build();
    primary_account_id_ =
        AccountId::FromUserEmail(managed_profile_->GetProfileUserName());
    const user_manager::User* user =
        fake_user_manager_->AddPublicAccountUser(primary_account_id_);
    fake_user_manager_->UserLoggedIn(primary_account_id_, user->username_hash(),
                                     false /* browser_restart */,
                                     false /* is_child */);
    fake_user_manager_->SetIsCurrentUserNew(/*is_new=*/true);

    // The AlwaysOnVpnPreConnectUrlAllowlistService keyed service for this
    // profile needs to be disassociated with the null object so that a new
    // instance of the service is created.
    ash::AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetInstance()
        ->RecreateServiceInstanceForTesting(managed_profile_.get());

    // The service should be created for a managed profile.
    ASSERT_TRUE(
        ash::AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetForProfile(
            managed_profile()));
  }

  void TearDownOnMainThread() override {
    fake_user_manager_->RemoveUserFromList(primary_account_id_);
    managed_profile_.reset();
    fake_user_manager_.Reset();
    AlwaysOnVpnPreConnectUrlAllowlistServiceTest::TearDownOnMainThread();
  }

 protected:
  void SetVpnConnectionState(bool vpn_enabled) {
    ShillServiceClient::TestInterface* service =
        ShillServiceClient::Get()->GetTestInterface();
    service->SetServiceProperty(
        kDefaultEthernetPath, shill::kStateProperty,
        base::Value(vpn_enabled ? shill::kStateIdle : shill::kStateOnline));
    service->SetServiceProperty(
        kDefaultVpnPath, shill::kStateProperty,
        base::Value(vpn_enabled ? shill::kStateOnline : shill::kStateIdle));
    base::RunLoop().RunUntilIdle();
  }

  // The kAlwaysOnVpnPreConnectUrlAllowlist should be enfoced when the following
  // conditions meet:
  // - The kAlwaysOnVpnPreConnectUrlAllowlist list is not empty;
  // - AlwaysOn VPN is active on the device (.i.e. the
  // arc::prefs::kAlwaysOnVpnLockdown pref is true);
  // - The VPN is not yet connected.
  void CreatePreConnectListEnv() {
    SetVpnConnectionState(/*vpn_enabled=*/false);
    const ash::NetworkState* network =
        ash::NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    EXPECT_NE(network->GetNetworkTechnologyType(),
              ash::NetworkState::NetworkTechnologyType::kVPN);
    base::Value::List list;
    list.Append(kTestUrl);
    managed_profile()->GetPrefs()->SetList(
        policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist,
        std::move(list));
    managed_profile()->GetPrefs()->SetBoolean(arc::prefs::kAlwaysOnVpnLockdown,
                                              true);
  }

  bool IsPreConnectListEnforced() {
    return ash::AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetForProfile(
               managed_profile())
        ->enforce_alwayson_pre_connect_url_allowlist();
  }

  Profile* managed_profile() { return managed_profile_.get(); }

 private:
  std::unique_ptr<::ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<AlwaysOnVpnPreConnectUrlAllowlistService>
      alwayson_vpn_pre_connect_url_allowlist_service_;
  std::unique_ptr<TestingProfile> managed_profile_;

  base::ScopedTempDir temp_dir_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  AccountId primary_account_id_;
};

// Ensure that the local state pref kEnforceAlwaysOnVpnPreConnectUrlAllowlist is
// true when the kAlwaysOnVpnPreConnectUrlAllowlist is set but the AlwaysOn VPN
// is not yet connected.
IN_PROC_BROWSER_TEST_F(
    AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest,
    PreConnectListEnabled) {
  EXPECT_FALSE(IsPreConnectListEnforced());
  CreatePreConnectListEnv();
  EXPECT_TRUE(IsPreConnectListEnforced());
}

// Ensure that the local state pref kEnforceAlwaysOnVpnPreConnectUrlAllowlist is
// false when the VPN is connected.
IN_PROC_BROWSER_TEST_F(
    AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest,
    VpnConnected) {
  CreatePreConnectListEnv();
  EXPECT_TRUE(IsPreConnectListEnforced());

  // Simulate the VPN state = connected.
  SetVpnConnectionState(/*vpn_enabled*/ true);
  EXPECT_FALSE(IsPreConnectListEnforced());

  // Simulate the VPN state = disconnected and check that the pre-connect list
  // is again enfoced.
  SetVpnConnectionState(/*vpn_enabled*/ false);
  EXPECT_TRUE(IsPreConnectListEnforced());
}

// Ensure that the local state pref kEnforceAlwaysOnVpnPreConnectUrlAllowlist is
// false when the kAlwaysOnVpnPreConnectUrlAllowlist pref is not set.
IN_PROC_BROWSER_TEST_F(
    AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest,
    AlwaysOnVpnPreConnectUrlAllowlistEmpty) {
  CreatePreConnectListEnv();
  EXPECT_TRUE(IsPreConnectListEnforced());

  // Create and set an empty list.
  base::Value::List list;
  managed_profile()->GetPrefs()->SetList(
      policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist, list.Clone());
  EXPECT_FALSE(IsPreConnectListEnforced());

  // Set a value for the kAlwaysOnVpnPreConnectUrlAllowlist pref again and
  // verify that the pre-connect list is again enforced.
  list.Append(kTestUrl);
  managed_profile()->GetPrefs()->SetList(
      policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist,
      std::move(list));
  EXPECT_TRUE(IsPreConnectListEnforced());
}

// Ensure that the local state pref kEnforceAlwaysOnVpnPreConnectUrlAllowlist is
// false when the VPN is not in lockdown mode.
IN_PROC_BROWSER_TEST_F(
    AlwaysOnVpnPreConnectUrlAllowlistServiceManagedProfileTest,
    AlwaysOnVpnFalse) {
  CreatePreConnectListEnv();
  EXPECT_TRUE(IsPreConnectListEnforced());

  // Remove lockdown mode.
  managed_profile()->GetPrefs()->SetBoolean(arc::prefs::kAlwaysOnVpnLockdown,
                                            false);
  EXPECT_FALSE(IsPreConnectListEnforced());

  // Set lockdown mode and verify that the pre-connect list is again
  // enforced.
  managed_profile()->GetPrefs()->SetBoolean(arc::prefs::kAlwaysOnVpnLockdown,
                                            true);
  EXPECT_TRUE(IsPreConnectListEnforced());
}

}  // namespace ash
