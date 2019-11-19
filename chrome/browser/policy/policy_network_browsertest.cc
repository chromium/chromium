// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_switches.h"
#endif

namespace {

bool IsQuicEnabled(network::mojom::NetworkContext* network_context) {
  GURL url = net::QuicSimpleTestServer::GetFileURL(
      net::QuicSimpleTestServer::GetHelloPath());
  int rv = content::LoadBasicRequest(network_context, url);
  return rv == net::OK;
}

bool IsQuicEnabled(Profile* profile) {
  return IsQuicEnabled(
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetNetworkContext());
}

bool IsQuicEnabledForSystem() {
  return IsQuicEnabled(
      g_browser_process->system_network_context_manager()->GetContext());
}

bool IsQuicEnabledForSafeBrowsing() {
  return IsQuicEnabled(
      g_browser_process->safe_browsing_service()->GetNetworkContext());
}

// Called when an additional profile has been created.
// The created profile is stored in *|out_created_profile|.
void OnProfileInitialized(Profile** out_created_profile,
                          const base::Closure& closure,
                          Profile* profile,
                          Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    *out_created_profile = profile;
    closure.Run();
  }
}

}  // namespace

namespace policy {

class QuicTestBase : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kOriginToForceQuicOn, "*");
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ConfigureMockCertVerifier();
    net::QuicSimpleTestServer::Start();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  void ConfigureMockCertVerifier() {
    auto test_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "quic-chain.pem");
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = test_cert;
    verify_result.is_issued_by_known_root = true;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCert(
        test_cert, verify_result, net::OK);
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
};

// The tests are based on the assumption that command line flag kEnableQuic
// guarantees that QUIC protocol is enabled which is the case at the moment
// when these are being written.
class QuicAllowedPolicyTestBase : public QuicTestBase {
 public:
  QuicAllowedPolicyTestBase() : QuicTestBase() {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    QuicTestBase::SetUpInProcessBrowserTestFixture();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    PolicyMap values;
    GetQuicAllowedPolicy(&values);
    provider_.UpdateChromePolicy(values);
  }

  virtual void GetQuicAllowedPolicy(PolicyMap* values) = 0;

  // Crashes the network service and restarts the QUIC server. If the QUIC
  // server isn't restarted, requests will fail with ERR_QUIC_PROTOCOL_ERROR.
  // TODO(https://crbug.com/851532): The reason the server restart is needed is
  // unclear, but ideally that should be fixed.
  void CrashNetworkServiceAndRestartQuicServer() {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      net::QuicSimpleTestServer::Shutdown();
    }
    SimulateNetworkServiceCrash();
    ConfigureMockCertVerifier();
    ASSERT_TRUE(net::QuicSimpleTestServer::Start());
  }

 private:
  MockConfigurationPolicyProvider provider_;
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyTestBase);
};

// Policy QuicAllowed set to false.
class QuicAllowedPolicyIsFalse: public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsFalse() : QuicAllowedPolicyTestBase() {}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
    values->Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
                nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsFalse);
};

// It's important that all these tests be separate, as the first NetworkContext
// instantiated after the crash could re-disable QUIC globally itself, so can't
// just crash the network service once, and then test all network contexts in
// some particular order.

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsFalse, QuicDisallowedForSystem) {
  EXPECT_FALSE(IsQuicEnabledForSystem());

  // If using the network service, crash the service, and make sure QUIC is
  // still disabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkServiceAndRestartQuicServer();
    // Make sure the NetworkContext has noticed the pipe was closed.
    g_browser_process->system_network_context_manager()
        ->FlushNetworkInterfaceForTesting();
    EXPECT_FALSE(IsQuicEnabledForSystem());
  }
}

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsFalse,
                       QuicDisallowedForSafeBrowsing) {
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());

  // If using the network service, crash the service, and make sure QUIC is
  // still disabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkServiceAndRestartQuicServer();
    // Make sure the NetworkContext has noticed the pipe was closed.
    g_browser_process->safe_browsing_service()
        ->FlushNetworkInterfaceForTesting();
    EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  }
}

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsFalse, QuicDisallowedForProfile) {
  EXPECT_FALSE(IsQuicEnabled(browser()->profile()));

  // If using the network service, crash the service, and make sure QUIC is
  // still disabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkServiceAndRestartQuicServer();
    // Make sure the NetworkContext has noticed the pipe was closed.
    content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
        ->FlushNetworkInterfaceForTesting();
    EXPECT_FALSE(IsQuicEnabled(browser()->profile()));
  }
}

// Policy QuicAllowed set to true.
class QuicAllowedPolicyIsTrue: public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsTrue() : QuicAllowedPolicyTestBase() {}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
    values->Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true),
                nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsTrue);
};

// It's important that all these tests be separate, as the first NetworkContext
// instantiated after the crash could re-disable QUIC globally itself, so can't
// just crash the network service once, and then test all network contexts in
// some particular order.

// TODO(crbug.com/938139): Flaky on ChromeOS with Network Service
#if defined(OS_CHROMEOS)
#define MAYBE_QuicAllowedForSystem DISABLED_QuicAllowedForSystem
#else
#define MAYBE_QuicAllowedForSystem QuicAllowedForSystem
#endif
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsTrue, MAYBE_QuicAllowedForSystem) {
  EXPECT_TRUE(IsQuicEnabledForSystem());

  // If using the network service, crash the service, and make sure QUIC is
  // still enabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkServiceAndRestartQuicServer();
    // Make sure the NetworkContext has noticed the pipe was closed.
    g_browser_process->system_network_context_manager()
        ->FlushNetworkInterfaceForTesting();
    EXPECT_TRUE(IsQuicEnabledForSystem());
  }
}

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsTrue, QuicAllowedForSafeBrowsing) {
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing());

  // If using the network service, crash the service, and make sure QUIC is
  // still enabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkServiceAndRestartQuicServer();
    // Make sure the NetworkContext has noticed the pipe was closed.
    g_browser_process->safe_browsing_service()
        ->FlushNetworkInterfaceForTesting();
    EXPECT_TRUE(IsQuicEnabledForSafeBrowsing());
  }
}

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsTrue, QuicAllowedForProfile) {
  EXPECT_TRUE(IsQuicEnabled(browser()->profile()));

  // If using the network service, crash the service, and make sure QUIC is
  // still enabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkServiceAndRestartQuicServer();
    // Make sure the NetworkContext has noticed the pipe was closed.
    content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
        ->FlushNetworkInterfaceForTesting();
    EXPECT_TRUE(IsQuicEnabled(browser()->profile()));
  }
}

// Policy QuicAllowed is not set.
class QuicAllowedPolicyIsNotSet : public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsNotSet() : QuicAllowedPolicyTestBase() {}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsNotSet);
};

// Flaky test on Win7. https://crbug.com/961049
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsNotSet, DISABLED_NoQuicRegulations) {
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing());
  EXPECT_TRUE(IsQuicEnabled(browser()->profile()));
}

// Policy QuicAllowed is set dynamically after profile creation.
// Supports creation of an additional profile.
class QuicAllowedPolicyDynamicTest : public QuicTestBase {
 public:
  QuicAllowedPolicyDynamicTest() : profile_1_(nullptr), profile_2_(nullptr) {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
    // Ensure that QUIC is enabled by default on browser startup.
    command_line->AppendSwitch(switches::kEnableQuic);
    QuicTestBase::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    QuicTestBase::SetUpInProcessBrowserTestFixture();
    // Set the overriden policy provider for the first Profile (profile_1_).
    EXPECT_CALL(policy_for_profile_1_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::PushProfilePolicyConnectorProviderForTesting(
        &policy_for_profile_1_);
  }

  void SetUpOnMainThread() override {
    profile_1_ = browser()->profile();
    QuicTestBase::SetUpOnMainThread();
  }

  // Creates a second Profile for testing. The Profile can then be accessed by
  // profile_2() and its policy by policy_for_profile_2().
  void CreateSecondProfile() {
    EXPECT_FALSE(profile_2_);

    // Prepare policy provider for second profile.
    EXPECT_CALL(policy_for_profile_2_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::PushProfilePolicyConnectorProviderForTesting(
        &policy_for_profile_2_);

    ProfileManager* profile_manager = g_browser_process->profile_manager();

    // Create an additional profile.
    base::FilePath path_profile =
        profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        path_profile,
        base::Bind(&OnProfileInitialized, &profile_2_, run_loop.QuitClosure()),
        base::string16(), std::string());

    // Run the message loop to allow profile creation to take place; the loop is
    // terminated by OnProfileInitialized calling the loop's QuitClosure when
    // the profile is created.
    run_loop.Run();

    // Make sure second profile creation does what we think it does.
    EXPECT_TRUE(profile_1() != profile_2());
  }

  // Sets the QuicAllowed policy for a Profile.
  // |provider| is supposed to be the MockConfigurationPolicyProvider for the
  // Profile, as returned by policy_for_profile_1() / policy_for_profile_2().
  void SetQuicAllowedPolicy(MockConfigurationPolicyProvider* provider,
                            bool value) {
    PolicyMap policy_map;
    policy_map.Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(value),
                   nullptr);
    provider->UpdateChromePolicy(policy_map);
    base::RunLoop().RunUntilIdle();

    // To avoid any races between checking the status and disabling QUIC, flush
    // the NetworkService Mojo interface, which is the one that has the
    // DisableQuic() method.
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Removes all policies for a Profile.
  // |provider| is supposed to be the MockConfigurationPolicyProvider for the
  // Profile, as returned by policy_for_profile_1() / policy_for_profile_2().
  void RemoveAllPolicies(MockConfigurationPolicyProvider* provider) {
    PolicyMap policy_map;
    provider->UpdateChromePolicy(policy_map);
    base::RunLoop().RunUntilIdle();

    // To avoid any races between sending future requests and disabling QUIC in
    // the network process, flush the NetworkService Mojo interface, which is
    // the one that has the DisableQuic() method.
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Returns the first Profile.
  Profile* profile_1() { return profile_1_; }

  // Returns the second Profile. May only be called after CreateSecondProfile
  // has been called.
  Profile* profile_2() {
    // Only valid after CreateSecondProfile() has been called.
    EXPECT_TRUE(profile_2_);
    return profile_2_;
  }

  // Returns the MockConfigurationPolicyProvider for profile_1.
  MockConfigurationPolicyProvider* policy_for_profile_1() {
    return &policy_for_profile_1_;
  }

  // Returns the MockConfigurationPolicyProvider for profile_2.
  MockConfigurationPolicyProvider* policy_for_profile_2() {
    return &policy_for_profile_2_;
  }

 private:
  // The first profile.
  Profile* profile_1_;
  // The second profile. Only valid after CreateSecondProfile() has been called.
  Profile* profile_2_;

  // Mock Policy for profile_1_.
  MockConfigurationPolicyProvider policy_for_profile_1_;
  // Mock Policy for profile_2_.
  MockConfigurationPolicyProvider policy_for_profile_2_;

  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyDynamicTest);
};

// QUIC is disallowed by policy after the profile has been initialized.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest, QuicAllowedFalseThenTrue) {
  // After browser start, QuicAllowed=false comes in dynamically
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));

  // Set the QuicAllowed policy to true again
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  // Effectively, QUIC is still disabled because QUIC re-enabling is not
  // supported.
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));

  // Completely remove the QuicAllowed policy
  RemoveAllPolicies(policy_for_profile_1());
  // Effectively, QUIC is still disabled because QUIC re-enabling is not
  // supported.
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));

  // QuicAllowed=false is set again
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
}

// QUIC is allowed, then disallowed by policy after the profile has been
// initialized.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest,
                       DISABLED_QuicAllowedTrueThenFalse) {
  // After browser start, QuicAllowed=true comes in dynamically
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing());
  EXPECT_TRUE(IsQuicEnabled(profile_1()));

  // Completely remove the QuicAllowed policy
  RemoveAllPolicies(policy_for_profile_1());
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing());
  EXPECT_TRUE(IsQuicEnabled(profile_1()));

  // Set the QuicAllowed policy to true again
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing());
  EXPECT_TRUE(IsQuicEnabled(profile_1()));

  // Now set QuicAllowed=false
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
}

// A second Profile is created when QuicAllowed=false policy is in effect for
// the first profile.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest,
                       SecondProfileCreatedWhenQuicAllowedFalse) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));

  CreateSecondProfile();

  // QUIC is disabled in both profiles
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
  EXPECT_FALSE(IsQuicEnabled(profile_2()));
}

// A second Profile is created when no QuicAllowed policy is in effect for the
// first profile.
// Then QuicAllowed=false policy is dynamically set for both profiles.
//
// Disabled due to flakiness on windows: https://crbug.com/947931.
#if defined(OS_WIN)
#define MAYBE_QuicAllowedFalseAfterTwoProfilesCreated \
  DISABLED_QuicAllowedFalseAfterTwoProfilesCreated
#else
#define MAYBE_QuicAllowedFalseAfterTwoProfilesCreated \
  QuicAllowedFalseAfterTwoProfilesCreated
#endif
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest,
                       MAYBE_QuicAllowedFalseAfterTwoProfilesCreated) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  CreateSecondProfile();

  // QUIC is enabled in both profiles
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing());
  EXPECT_TRUE(IsQuicEnabled(profile_1()));
  EXPECT_TRUE(IsQuicEnabled(profile_2()));

  // Disable QUIC in first profile
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
  EXPECT_FALSE(IsQuicEnabled(profile_2()));

  // Disable QUIC in second profile
  SetQuicAllowedPolicy(policy_for_profile_2(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing());
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
  EXPECT_FALSE(IsQuicEnabled(profile_2()));
}

}  // namespace policy
