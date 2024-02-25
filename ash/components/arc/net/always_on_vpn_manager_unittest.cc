// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/always_on_vpn_manager.h"

#include <string_view>

#include "ash/components/arc/arc_prefs.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

constexpr const char kVpnPackage[] = "com.android.vpn";
const base::Value kVpnPackageValue(kVpnPackage);

void OnGetProperties(bool* success_out,
                     std::string* package_name_out,
                     base::OnceClosure callback,
                     std::optional<base::Value::Dict> result) {
  *success_out = result.has_value();
  if (result) {
    const std::string* value =
        result->FindString(shill::kAlwaysOnVpnPackageProperty);
    if (value != nullptr)
      *package_name_out = *value;
  }
  std::move(callback).Run();
}

std::string GetAlwaysOnPackageName() {
  bool success = false;
  std::string package_name;
  ash::ShillManagerClient* shill_manager = ash::ShillManagerClient::Get();
  base::RunLoop run_loop;
  shill_manager->GetProperties(
      base::BindOnce(&OnGetProperties, base::Unretained(&success),
                     base::Unretained(&package_name), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
  return package_name;
}

}  // namespace

namespace arc {
namespace {

class AlwaysOnVpnManagerTest : public testing::Test {
 public:
  AlwaysOnVpnManagerTest() = default;

  AlwaysOnVpnManagerTest(const AlwaysOnVpnManagerTest&) = delete;
  AlwaysOnVpnManagerTest& operator=(const AlwaysOnVpnManagerTest&) = delete;

  void SetUp() override {
    arc::prefs::RegisterProfilePrefs(pref_service()->registry());
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(AlwaysOnVpnManagerTest, SetPackageWhileLockdownUnset) {
  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(
      pref_service(), /*delay_lockdown_until_vpn_connected=*/false);

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());
}

TEST_F(AlwaysOnVpnManagerTest, SetPackageWhileLockdownTrue) {
  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(true));

  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(
      pref_service(), /*delay_lockdown_until_vpn_connected=*/false);

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);

  EXPECT_EQ(kVpnPackage, GetAlwaysOnPackageName());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage,
                      base::Value(std::string_view()));

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());
}

TEST_F(AlwaysOnVpnManagerTest, SetPackageThatsAlreadySetAtBoot) {
  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(true));
  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);

  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(
      pref_service(), /*delay_lockdown_until_vpn_connected=*/false);

  EXPECT_EQ(kVpnPackage, GetAlwaysOnPackageName());
}

TEST_F(AlwaysOnVpnManagerTest, SetLockdown) {
  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);

  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(
      pref_service(), /*delay_lockdown_until_vpn_connected=*/false);

  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(true));

  EXPECT_EQ(kVpnPackage, GetAlwaysOnPackageName());

  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(false));

  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());
}

// Verify that the shill::kAlwaysOnVpnPackageProperty property is not set if the
// browser user traffic is restricted by the AlwaysOnVpnPreConnectUrlAllowlist
// preference.
TEST_F(AlwaysOnVpnManagerTest, EnforceAlwaysOnVpnPreConnectUrlAllowlist) {
  auto always_on_manager = std::make_unique<AlwaysOnVpnManager>(
      pref_service(), /*delay_lockdown_until_vpn_connected=*/false);

  pref_service()->Set(arc::prefs::kAlwaysOnVpnLockdown, base::Value(true));
  pref_service()->Set(arc::prefs::kAlwaysOnVpnPackage, kVpnPackageValue);
  EXPECT_EQ(kVpnPackage, GetAlwaysOnPackageName());

  always_on_manager->SetDelayLockdownUntilVpnConnectedState(/*enabled=*/true);
  EXPECT_EQ(std::string(), GetAlwaysOnPackageName());

  always_on_manager->SetDelayLockdownUntilVpnConnectedState(
      /*enabled=*/false);
  EXPECT_EQ(kVpnPackage, GetAlwaysOnPackageName());
}

}  // namespace
}  // namespace arc
