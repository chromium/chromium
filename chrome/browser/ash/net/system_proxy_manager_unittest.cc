// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/system_proxy_manager.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_service.pb.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace ash {

namespace {

constexpr char kBrowserUsername[] = "browser_username";
constexpr char16_t kBrowserUsername16[] = u"browser_username";
constexpr char kBrowserPassword[] = "browser_password";
constexpr char16_t kBrowserPassword16[] = u"browser_password";
constexpr char kPolicyUsername[] = "policy_username";
constexpr char kPolicyPassword[] = "policy_password";
constexpr char kKerberosActivePrincipalName[] = "kerberos_princ_name";
constexpr char kProxyAuthUrl[] = "http://example.com:3128";
constexpr char kProxyAuthEmptyPath[] = "http://example.com:3128/";
constexpr char kRealm[] = "My proxy";
constexpr char kScheme[] = "dIgEsT";
constexpr char kProxyAuthChallenge[] = "challenge";
constexpr char kLocalProxyAddress[] = "local-proxy.com:3128";

std::unique_ptr<network::NetworkContext>
CreateNetworkContextForDefaultStoragePartition(
    network::NetworkService* network_service,
    content::BrowserContext* browser_context) {
  mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
  auto params = network::mojom::NetworkContextParams::New();
  params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  auto network_context = std::make_unique<network::NetworkContext>(
      network_service, network_context_remote.InitWithNewPipeAndPassReceiver(),
      std::move(params));
  browser_context->GetDefaultStoragePartition()->SetNetworkContextForTesting(
      std::move(network_context_remote));
  return network_context;
}

network::NetworkService* GetNetworkService() {
  content::GetNetworkService();
  // Wait for the Network Service to initialize on the IO thread.
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  return network::NetworkService::GetNetworkServiceForTesting();
}

void SetManagedProxy(Profile* profile) {
  // Configure a proxy via user policy.
  auto proxy_config = base::Value::Dict()
                          .Set("mode", ProxyPrefs::kFixedServersProxyModeName)
                          .Set("server", kProxyAuthUrl);
  profile->GetPrefs()->SetDict(proxy_config::prefs::kProxy,
                               std::move(proxy_config));
  base::RunLoop().RunUntilIdle();
}

net::AuthChallengeInfo GetAuthInfo() {
  net::AuthChallengeInfo auth_info;
  auth_info.is_proxy = true;
  auth_info.scheme = kScheme;
  return auth_info;
}

}  // namespace

// TODO(acostinas, https://crbug.com/1102351) Replace RunUntilIdle() in tests
// with RunLoop::Run() with explicit RunLoop::QuitClosure().
class SystemProxyManagerTest : public testing::Test {
 public:
  SystemProxyManagerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~SystemProxyManagerTest() override = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    LoginState::Initialize();

    profile_ = std::make_unique<TestingProfile>();
    SystemProxyClient::InitializeFake();
    system_proxy_manager_ =
        std::make_unique<SystemProxyManager>(local_state_.Get());
    // Listen for pref changes for the primary profile.
    system_proxy_manager_->StartObservingPrimaryProfilePrefs(profile_.get());
    NetworkHandler::Get()->InitializePrefServices(profile_->GetPrefs(),
                                                  local_state_.Get());
  }

  void TearDown() override {
    system_proxy_manager_->StopObservingPrimaryProfilePrefs();
    system_proxy_manager_.reset();
    LoginState::Shutdown();
    SystemProxyClient::Shutdown();
    network_handler_test_helper_.reset();
  }

 protected:
  void SetPolicy(bool system_proxy_enabled,
                 const std::string& system_services_username,
                 const std::string& system_services_password) {
    system_proxy_manager_->SetPolicySettings(
        system_proxy_enabled, system_services_username,
        system_services_password, /*auth_schemes=*/{});
  }

  SystemProxyClient::TestInterface* client_test_interface() {
    return SystemProxyClient::Get()->GetTestInterface();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<SystemProxyManager> system_proxy_manager_;
  std::unique_ptr<TestingProfile> profile_;
};

// Tests that |SystemProxyManager| sends the correct Kerberos details and
// updates to System-proxy.
TEST_F(SystemProxyManagerTest, KerberosConfig) {
  int expected_set_auth_details_call_count = 0;
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  local_state_.Get()->SetBoolean(prefs::kKerberosEnabled, true);
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_FALSE(request.has_credentials());
  EXPECT_TRUE(request.kerberos_enabled());
  EXPECT_EQ(request.traffic_type(), system_proxy::TrafficOrigin::SYSTEM);

  // Set an active principal name.
  profile_->GetPrefs()->SetString(prefs::kKerberosActivePrincipalName,
                                  kKerberosActivePrincipalName);
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_EQ(kKerberosActivePrincipalName, request.active_principal_name());
  EXPECT_EQ(request.traffic_type(), system_proxy::TrafficOrigin::ALL);

  // Remove the active principal name.
  profile_->GetPrefs()->SetString(prefs::kKerberosActivePrincipalName, "");
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_EQ("", request.active_principal_name());
  EXPECT_TRUE(request.kerberos_enabled());

  // Disable kerberos.
  local_state_.Get()->SetBoolean(prefs::kKerberosEnabled, false);
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_FALSE(request.kerberos_enabled());
}

// Tests that when no user is signed in, credential requests are resolved to a
// D-Bus call which sends back to System-proxy empty credentials for the
// specified protection space.
TEST_F(SystemProxyManagerTest, UserCredentialsRequiredNoUser) {
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  system_proxy_manager_->StopObservingPrimaryProfilePrefs();
  system_proxy::ProtectionSpace protection_space;
  protection_space.set_origin(kProxyAuthUrl);
  protection_space.set_scheme(kScheme);
  protection_space.set_realm(kRealm);

  system_proxy::AuthenticationRequiredDetails details;
  details.set_bad_cached_credentials(false);
  *details.mutable_proxy_protection_space() = protection_space;

  client_test_interface()->SendAuthenticationRequiredSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_protection_space());
  ASSERT_EQ(protection_space.SerializeAsString(),
            request.protection_space().SerializeAsString());

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ("", request.credentials().username());
  EXPECT_EQ("", request.credentials().password());
  system_proxy_manager_->StartObservingPrimaryProfilePrefs(profile_.get());
}

// Tests that credential requests are resolved to a  D-Bus call which sends back
// to System-proxy credentials acquired from the NetworkService.
TEST_F(SystemProxyManagerTest, UserCredentialsRequestedFromNetworkService) {
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  // Setup the NetworkContext with credentials.
  std::unique_ptr<network::NetworkContext> network_context =
      CreateNetworkContextForDefaultStoragePartition(GetNetworkService(),
                                                     profile_.get());
  network_context->url_request_context()
      ->http_transaction_factory()
      ->GetSession()
      ->http_auth_cache()
      ->Add(url::SchemeHostPort(GURL(kProxyAuthEmptyPath)),
            net::HttpAuth::AUTH_PROXY, kRealm,
            net::HttpAuth::AUTH_SCHEME_DIGEST, net::NetworkAnonymizationKey(),
            kProxyAuthChallenge,
            net::AuthCredentials(kBrowserUsername16, kBrowserPassword16),
            std::string() /* path */);

  system_proxy::ProtectionSpace protection_space;
  protection_space.set_origin(kProxyAuthUrl);
  protection_space.set_scheme(kScheme);
  protection_space.set_realm(kRealm);

  system_proxy::AuthenticationRequiredDetails details;
  *details.mutable_proxy_protection_space() = protection_space;

  EXPECT_EQ(1, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  client_test_interface()->SendAuthenticationRequiredSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_protection_space());
  EXPECT_EQ(protection_space.SerializeAsString(),
            request.protection_space().SerializeAsString());

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ(kBrowserUsername, request.credentials().username());
  EXPECT_EQ(kBrowserPassword, request.credentials().password());
  EXPECT_EQ(request.traffic_type(), system_proxy::TrafficOrigin::SYSTEM);

  // Enable ARC and verify that the credentials are sent both for user and
  // system traffic.
  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  task_environment_.RunUntilIdle();
  client_test_interface()->SendAuthenticationRequiredSignal(details);
  task_environment_.RunUntilIdle();
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_EQ(request.traffic_type(), system_proxy::TrafficOrigin::ALL);
}

// Tests that |SystemProxyManager| sends requests to start and shut down the
// worker which tunnels ARC++ traffic according to policy.
TEST_F(SystemProxyManagerTest, EnableArcWorker) {
  int expected_set_auth_details_call_count = 0;
  int expected_shutdown_calls = 0;
  EXPECT_EQ(expected_shutdown_calls,
            client_test_interface()->GetShutDownCallCount());

  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, false);
  EXPECT_EQ(++expected_shutdown_calls,
            client_test_interface()->GetShutDownCallCount());
}

// Tests that the user preference used by ARC++ to point to the local proxy is
// kept in sync.
TEST_F(SystemProxyManagerTest, ArcWorkerAddressPrefSynced) {
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  system_proxy::WorkerActiveSignalDetails details;
  details.set_traffic_origin(system_proxy::TrafficOrigin::USER);
  details.set_local_proxy_url(kLocalProxyAddress);
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kLocalProxyAddress,
            profile_->GetPrefs()->GetString(
                ::prefs::kSystemProxyUserTrafficHostAndPort));

  // The preference shouldn't be updated if the signal is send for system
  // traffic.
  details.set_traffic_origin(system_proxy::TrafficOrigin::SYSTEM);
  details.set_local_proxy_url("other address");
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kLocalProxyAddress,
            profile_->GetPrefs()->GetString(
                ::prefs::kSystemProxyUserTrafficHostAndPort));

  SetPolicy(false /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetString(::prefs::kSystemProxyUserTrafficHostAndPort)
                  .empty());
}

// Verifies that only MGS and Kiosk can use the policy provided credentials.
TEST_F(SystemProxyManagerTest, CanUsePolicyCredentialsUserType) {
  SetPolicy(/*system_proxy_enabled=*/true,
            /*system_services_username=*/kPolicyUsername,
            /*system_services_password=*/kPolicyPassword);
  SetManagedProxy(profile_.get());

  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);

  EXPECT_TRUE(system_proxy_manager_->CanUsePolicyCredentials(
      GetAuthInfo(), /*first_auth_attempt=*/true));

  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_KIOSK);

  EXPECT_TRUE(system_proxy_manager_->CanUsePolicyCredentials(
      GetAuthInfo(), /*first_auth_attempt=*/true));

  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_REGULAR);

  EXPECT_FALSE(system_proxy_manager_->CanUsePolicyCredentials(
      GetAuthInfo(), /*first_auth_attempt=*/true));
}

// Verifies that the policy provided credentials are only used for proxy auth.
TEST_F(SystemProxyManagerTest, CanUsePolicyCredentialsOriginServer) {
  SetPolicy(/*system_proxy_enabled=*/true,
            /*system_services_username=*/kPolicyUsername,
            /*system_services_password=*/kPolicyPassword);
  SetManagedProxy(profile_.get());

  net::AuthChallengeInfo auth_info = GetAuthInfo();
  auth_info.is_proxy = false;
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);

  EXPECT_FALSE(system_proxy_manager_->CanUsePolicyCredentials(
      auth_info, /*first_auth_attempt=*/true));
}

// Verifies that the policy provided credentials are only used for managed
// proxies.
TEST_F(SystemProxyManagerTest, CanUsePolicyCredentialsNoManagedProxy) {
  SetPolicy(/*system_proxy_enabled=*/true,
            /*system_services_username=*/kPolicyUsername,
            /*system_services_password=*/kPolicyPassword);

  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);

  EXPECT_FALSE(system_proxy_manager_->CanUsePolicyCredentials(
      GetAuthInfo(), /*first_auth_attempt=*/true));
}

// Verifies that `CanUsePolicyCredentials` returns false if no credentials are
// specified by policy.
TEST_F(SystemProxyManagerTest, NoPolicyCredentials) {
  SetPolicy(/*system_proxy_enabled=*/true,
            /*system_services_username=*/"",
            /*system_services_password=*/"");
  SetManagedProxy(profile_.get());

  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);

  EXPECT_FALSE(system_proxy_manager_->CanUsePolicyCredentials(
      GetAuthInfo(), /*first_auth_attempt=*/true));
}

// Verifies that `CanUsePolicyCredentials` is only returning true for the first
// auth attempt.
TEST_F(SystemProxyManagerTest, CanUsePolicyCredentialsMgsMaxTries) {
  SetPolicy(/*system_proxy_enabled=*/true,
            /*system_services_username=*/kPolicyUsername,
            /*system_services_password=*/kPolicyPassword);
  SetManagedProxy(profile_.get());

  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  EXPECT_TRUE(system_proxy_manager_->CanUsePolicyCredentials(
      GetAuthInfo(), /*first_auth_attempt=*/true));
  EXPECT_FALSE(system_proxy_manager_->CanUsePolicyCredentials(
      GetAuthInfo(), /*first_auth_attempt=*/false));
}

TEST_F(SystemProxyManagerTest, SystemServicesProxyPacStringDefault) {
  SetPolicy(/*system_proxy_enabled=*/true,
            /*system_services_username=*/kPolicyUsername,
            /*system_services_password=*/kPolicyPassword);
  system_proxy::WorkerActiveSignalDetails details;
  details.set_traffic_origin(system_proxy::TrafficOrigin::SYSTEM);
  details.set_local_proxy_url(kProxyAuthUrl);
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(system_proxy_manager_->SystemServicesProxyPacString(
                chromeos::SystemProxyOverride::kDefault),
            "PROXY http://example.com:3128");
}

TEST_F(SystemProxyManagerTest, SystemServicesProxyPacStringOptOut) {
  SetPolicy(/*system_proxy_enabled=*/true,
            /*system_services_username=*/kPolicyUsername,
            /*system_services_password=*/kPolicyPassword);
  system_proxy::WorkerActiveSignalDetails details;
  details.set_traffic_origin(system_proxy::TrafficOrigin::SYSTEM);
  details.set_local_proxy_url(kProxyAuthUrl);
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      system_proxy_manager_
          ->SystemServicesProxyPacString(chromeos::SystemProxyOverride::kOptOut)
          .empty());
}

// Tests the behaviour of SystemProxyManager when enabled via the feature flag
// `features::kSystemProxyForSystemServices`.
class FeatureEnabledSystemProxyTest : public SystemProxyManagerTest {
 public:
  FeatureEnabledSystemProxyTest() : SystemProxyManagerTest() {}
  ~FeatureEnabledSystemProxyTest() override = default;

  // testing::Test
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSystemProxyForSystemServices);
    SystemProxyManagerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that system services get the address of the local proxy worker for
// system services.
TEST_F(FeatureEnabledSystemProxyTest, SystemServicesDefault) {
  system_proxy::WorkerActiveSignalDetails details;
  details.set_traffic_origin(system_proxy::TrafficOrigin::SYSTEM);
  details.set_local_proxy_url(kLocalProxyAddress);
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(system_proxy_manager_
                  ->SystemServicesProxyPacString(
                      chromeos::SystemProxyOverride::kDefault)
                  .empty());
}

TEST_F(FeatureEnabledSystemProxyTest, SystemServicesOptIn) {
  system_proxy::WorkerActiveSignalDetails details;
  details.set_traffic_origin(system_proxy::TrafficOrigin::SYSTEM);
  details.set_local_proxy_url(kLocalProxyAddress);
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(system_proxy_manager_->SystemServicesProxyPacString(
                chromeos::SystemProxyOverride::kOptIn),
            "PROXY local-proxy.com:3128");
}

// Tests that the pref which sets the local proxy worker address for ARC++ is
// not set when the flag is enabled.
TEST_F(FeatureEnabledSystemProxyTest, Arc) {
  system_proxy::WorkerActiveSignalDetails details;
  details.set_traffic_origin(system_proxy::TrafficOrigin::USER);
  details.set_local_proxy_url(kLocalProxyAddress);
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetString(::prefs::kSystemProxyUserTrafficHostAndPort)
                  .empty());
}

// Tests that enabling system-proxy via policy will still work as expected for
// ARC++.
TEST_F(FeatureEnabledSystemProxyTest, ArcPolicyEnabled) {
  SetPolicy(/*system_proxy_enabled=*/true,
            /*system_services_username=*/"",
            /*system_services_password=*/"");

  system_proxy::WorkerActiveSignalDetails details;
  details.set_traffic_origin(system_proxy::TrafficOrigin::USER);
  details.set_local_proxy_url(kLocalProxyAddress);
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(kLocalProxyAddress,
            profile_->GetPrefs()->GetString(
                ::prefs::kSystemProxyUserTrafficHostAndPort));
}

}  // namespace ash
