// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/system_proxy_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_service.pb.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace {
constexpr char kPolicyUsername[] = "policy_username";
constexpr char kPolicyPassword[] = "policy_password";
constexpr char kProxyAuthUrl[] = "http://example.com:3128";
}  // namespace

namespace policy {
// TODO(acostinas, https://crbug.com/1102351) Replace RunUntilIdle() in tests
// with RunLoop::Run() with explicit RunLoop::QuitClosure().
class SystemProxyHandlerTest : public testing::Test {
 public:
  SystemProxyHandlerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~SystemProxyHandlerTest() override = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
    ash::SystemProxyClient::InitializeFake();

    system_proxy_handler_ =
        std::make_unique<SystemProxyHandler>(ash::CrosSettings::Get());
    system_proxy_manager_ =
        std::make_unique<ash::SystemProxyManager>(local_state_.Get());
    profile_ = std::make_unique<TestingProfile>();
    system_proxy_manager_->StartObservingPrimaryProfilePrefs(profile_.get());

    system_proxy_handler_->SetSystemProxyManagerForTesting(
        system_proxy_manager_.get());
    ash::NetworkHandler::Get()->InitializePrefServices(profile_->GetPrefs(),
                                                       local_state_.Get());
  }

  void TearDown() override {
    system_proxy_manager_->StopObservingPrimaryProfilePrefs();
    system_proxy_manager_.reset();
    ash::SystemProxyClient::Shutdown();
    network_handler_test_helper_.reset();
  }

 protected:
  void SetPolicy(bool system_proxy_enabled,
                 const std::string& system_services_username,
                 const std::string& system_services_password) {
    auto dict = base::Value::Dict()
                    .Set("system_proxy_enabled", system_proxy_enabled)
                    .Set("system_services_username", system_services_username)
                    .Set("system_services_password", system_services_password);
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kSystemProxySettings, base::Value(std::move(dict)));
    task_environment_.RunUntilIdle();
  }

  void SetManagedProxy(Profile* profile) {
    // Configure a proxy via user policy.
    auto proxy_config = base::Value::Dict()
                            .Set("mode", ProxyPrefs::kFixedServersProxyModeName)
                            .Set("server", kProxyAuthUrl);
    profile->GetPrefs()->SetDict(proxy_config::prefs::kProxy,
                                 std::move(proxy_config));
    task_environment_.RunUntilIdle();
  }

  ash::SystemProxyClient::TestInterface* client_test_interface() {
    return ash::SystemProxyClient::Get()->GetTestInterface();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  ScopedTestingLocalState local_state_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SystemProxyHandler> system_proxy_handler_;
  std::unique_ptr<ash::SystemProxyManager> system_proxy_manager_;
};

// Verifies that authentication details are forwarded to system-proxy according
// to the |kSystemProxySettings| policy.
TEST_F(SystemProxyHandlerTest, SetAuthenticationDetails) {
  EXPECT_EQ(0, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  EXPECT_EQ(1, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  SetPolicy(true /* system_proxy_enabled */, kPolicyUsername, kPolicyPassword);
  EXPECT_EQ(2, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ("", request.credentials().username());
  EXPECT_EQ("", request.credentials().password());

  SetManagedProxy(profile_.get());
  EXPECT_EQ(3, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ(kPolicyUsername, request.credentials().username());
  EXPECT_EQ(kPolicyPassword, request.credentials().password());
}

// Verifies requests to shut down are sent to System-proxy according to the
// |kSystemProxySettings| policy.
TEST_F(SystemProxyHandlerTest, ShutDownDaemon) {
  int expected_shutdown_calls = 0;

  // Enable system-proxy.
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  EXPECT_EQ(expected_shutdown_calls,
            client_test_interface()->GetShutDownCallCount());

  // Disable system-proxy via policy and expect a shut-down request to be
  // sent.
  SetPolicy(false /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  EXPECT_EQ(++expected_shutdown_calls,
            client_test_interface()->GetShutDownCallCount());
}

}  // namespace policy
