// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_manager.h"

#include "base/barrier_closure.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/testing_wilco_dtc_supportd_bridge_wrapper.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_client.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::StrictMock;

namespace chromeos {

namespace {

// An implementation of Upstart Client that fakes a start/ stop of wilco DTC
// services on StartWilcoDtcService() / StopWilcoDtcService() calls.
class TestUpstartClient final : public FakeUpstartClient {
 public:
  // FakeUpstartClient overrides:
  void StartWilcoDtcService(
      chromeos::VoidDBusMethodCallback callback) override {
    std::move(callback).Run(result_);
  }

  void StopWilcoDtcService(chromeos::VoidDBusMethodCallback callback) override {
    std::move(callback).Run(result_);
  }

  // Sets the result to be passed into callbacks.
  void set_result(bool result) { result_ = result; }

 private:
  bool result_ = true;
};

class MockMojoWilcoDtcSupportdService
    : public wilco_dtc_supportd::mojom::WilcoDtcSupportdService {
 public:
  MOCK_METHOD2(SendUiMessageToWilcoDtc,
               void(mojo::ScopedHandle, SendUiMessageToWilcoDtcCallback));

  MOCK_METHOD0(NotifyConfigurationDataChanged, void());
};

// An implementation of the WilcoDtcSupportdManager::Delegate that owns the
// testing instance of the WilcoDtcSupportdBridge.
class FakeWilcoDtcSupportdManagerDelegate final
    : public WilcoDtcSupportdManager::Delegate {
 public:
  FakeWilcoDtcSupportdManagerDelegate(
      MockMojoWilcoDtcSupportdService* mojo_wilco_dtc_supportd_service)
      : mojo_wilco_dtc_supportd_service_(mojo_wilco_dtc_supportd_service) {}

  // WilcoDtcSupportdManager::Delegate overrides:
  std::unique_ptr<WilcoDtcSupportdBridge> CreateWilcoDtcSupportdBridge()
      override {
    std::unique_ptr<WilcoDtcSupportdBridge> wilco_dtc_supportd_bridge;
    testing_wilco_dtc_supportd_bridge_wrapper_ =
        TestingWilcoDtcSupportdBridgeWrapper::Create(
            mojo_wilco_dtc_supportd_service_,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            &wilco_dtc_supportd_bridge);
    DCHECK(wilco_dtc_supportd_bridge);
    testing_wilco_dtc_supportd_bridge_wrapper_->EstablishFakeMojoConnection();
    return wilco_dtc_supportd_bridge;
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingWilcoDtcSupportdBridgeWrapper>
      testing_wilco_dtc_supportd_bridge_wrapper_;
  MockMojoWilcoDtcSupportdService* mojo_wilco_dtc_supportd_service_;
};

// Tests WilcoDtcSupportdManager class instance.
class WilcoDtcSupportdManagerTest : public testing::Test {
 protected:
  WilcoDtcSupportdManagerTest() {
    WilcoDtcSupportdClient::InitializeFake();
    upstart_client_ = std::make_unique<TestUpstartClient>();
  }

  ~WilcoDtcSupportdManagerTest() override {
    WilcoDtcSupportdClient::Shutdown();
  }

  std::unique_ptr<WilcoDtcSupportdManager::Delegate> CreateDelegate() {
    return std::make_unique<FakeWilcoDtcSupportdManagerDelegate>(
        &mojo_wilco_dtc_supportd_service_);
  }

  void SetWilcoDtcAllowedPolicy(bool wilco_dtc_allowed) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kDeviceWilcoDtcAllowed, wilco_dtc_allowed);
  }

  void LogInUser(bool is_affiliated) {
    AccountId account_id =
        AccountId::FromUserEmail(user_manager::kStubUserEmail);
    fake_user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager_->LoginUser(account_id);
    session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  }

  MockMojoWilcoDtcSupportdService* mojo_wilco_dtc_supportd_service() {
    return &mojo_wilco_dtc_supportd_service_;
  }

  void SetUpstartClientResult(bool result) {
    upstart_client_->set_result(result);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<TestUpstartClient> upstart_client_;
  FakeChromeUserManager* fake_user_manager_{new FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_)};
  session_manager::SessionManager session_manager_;
  StrictMock<MockMojoWilcoDtcSupportdService> mojo_wilco_dtc_supportd_service_;
};

// Test that wilco DTC support services are not started on enterprise enrolled
// devices with a certain device policy unset.
TEST_F(WilcoDtcSupportdManagerTest, EnterpriseiWilcoDtcBasic) {
  WilcoDtcSupportdManager wilco_dtc_supportd_manager(CreateDelegate());
  EXPECT_FALSE(WilcoDtcSupportdBridge::Get());
}

// Test that wilco DTC support services are not started if disabled by device
// policy.
TEST_F(WilcoDtcSupportdManagerTest, EnterpriseWilcoDtcDisabled) {
  WilcoDtcSupportdManager wilco_dtc_supportd_manager(CreateDelegate());
  EXPECT_FALSE(WilcoDtcSupportdBridge::Get());

  SetWilcoDtcAllowedPolicy(false);
  EXPECT_FALSE(WilcoDtcSupportdBridge::Get());
}

// Test that wilco DTC support services are started if enabled by policy.
TEST_F(WilcoDtcSupportdManagerTest, EnterpriseWilcoDtcAllowed) {
  SetWilcoDtcAllowedPolicy(true);
  WilcoDtcSupportdManager wilco_dtc_supportd_manager(CreateDelegate());
  EXPECT_TRUE(WilcoDtcSupportdBridge::Get());
}

// Test that wilco DTC support services are not started if non-affiliated user
// is logged-in.
TEST_F(WilcoDtcSupportdManagerTest, EnterpriseNonAffiliatedUserLoggedIn) {
  WilcoDtcSupportdManager wilco_dtc_supportd_manager(CreateDelegate());
  EXPECT_FALSE(WilcoDtcSupportdBridge::Get());

  SetWilcoDtcAllowedPolicy(true);
  EXPECT_TRUE(WilcoDtcSupportdBridge::Get());

  LogInUser(false);
  EXPECT_FALSE(WilcoDtcSupportdBridge::Get());
}

// Test that wilco DTC support services are started if enabled by device policy
// and affiliated user is logged-in.
TEST_F(WilcoDtcSupportdManagerTest, EnterpriseAffiliatedUserLoggedIn) {
  SetWilcoDtcAllowedPolicy(true);
  WilcoDtcSupportdManager wilco_dtc_supportd_manager(CreateDelegate());
  EXPECT_TRUE(WilcoDtcSupportdBridge::Get());

  LogInUser(true);
  EXPECT_TRUE(WilcoDtcSupportdBridge::Get());
}

// Test that wilco DTC support services are not started if non-affiliated user
// is logged-in before the construction.
TEST_F(WilcoDtcSupportdManagerTest, EnterpriseNonAffiliatedUserLoggedInBefore) {
  SetWilcoDtcAllowedPolicy(true);
  LogInUser(false);
  WilcoDtcSupportdManager wilco_dtc_supportd_manager(CreateDelegate());

  EXPECT_FALSE(WilcoDtcSupportdBridge::Get());
}

// Test that wilco DTC support services are properly notified about the changes
// of configuration data.
TEST_F(WilcoDtcSupportdManagerTest, ConfigurationData) {
  constexpr char kFakeConfigurationData[] =
      "{\"fake-message\": \"Fake JSON configuration data\"}";

  WilcoDtcSupportdManager wilco_dtc_supportd_manager(CreateDelegate());
  EXPECT_FALSE(WilcoDtcSupportdBridge::Get());

  SetWilcoDtcAllowedPolicy(true);
  EXPECT_TRUE(WilcoDtcSupportdBridge::Get());
  // An empty configuration data by default.
  EXPECT_TRUE(
      WilcoDtcSupportdBridge::Get()->GetConfigurationDataForTesting().empty());

  // Set a non-empty configuration data.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mojo_wilco_dtc_supportd_service(),
                NotifyConfigurationDataChanged())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));

    wilco_dtc_supportd_manager.SetConfigurationData(
        std::make_unique<std::string>(kFakeConfigurationData));
    EXPECT_EQ(kFakeConfigurationData,
              WilcoDtcSupportdBridge::Get()->GetConfigurationDataForTesting());
    run_loop.Run();
  }

  // Restart the bridge.
  SetWilcoDtcAllowedPolicy(false);
  EXPECT_FALSE(WilcoDtcSupportdBridge::Get());
  SetWilcoDtcAllowedPolicy(true);
  EXPECT_TRUE(WilcoDtcSupportdBridge::Get());

  // The configuration data has not been changed.
  EXPECT_EQ(kFakeConfigurationData,
            WilcoDtcSupportdBridge::Get()->GetConfigurationDataForTesting());

  // Clear the configuration data.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mojo_wilco_dtc_supportd_service(),
                NotifyConfigurationDataChanged())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    wilco_dtc_supportd_manager.SetConfigurationData(nullptr);
    EXPECT_TRUE(WilcoDtcSupportdBridge::Get()
                    ->GetConfigurationDataForTesting()
                    .empty());
    run_loop.Run();
  }
}

// Test that Mojo bridge has been started even if the wilco DTC support
// services were already running.
TEST_F(WilcoDtcSupportdManagerTest, RunningUpstartJob) {
  SetWilcoDtcAllowedPolicy(true);
  SetUpstartClientResult(false);
  WilcoDtcSupportdManager wilco_dtc_supportd_manager(CreateDelegate());

  EXPECT_TRUE(WilcoDtcSupportdBridge::Get());
}

}  // namespace

}  // namespace chromeos
