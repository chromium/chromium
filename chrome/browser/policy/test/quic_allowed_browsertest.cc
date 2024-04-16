// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
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
      profile->GetDefaultStoragePartition()->GetNetworkContext());
}

bool IsQuicEnabledForSystem() {
  return IsQuicEnabled(
      g_browser_process->system_network_context_manager()->GetContext());
}

bool IsQuicEnabledForSafeBrowsing(Profile* profile) {
  return IsQuicEnabled(
      g_browser_process->safe_browsing_service()->GetNetworkContext(profile));
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
  QuicAllowedPolicyTestBase(const QuicAllowedPolicyTestBase&) = delete;
  QuicAllowedPolicyTestBase& operator=(const QuicAllowedPolicyTestBase&) =
      delete;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    QuicTestBase::SetUpInProcessBrowserTestFixture();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);

    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    PolicyMap values;
    GetQuicAllowedPolicy(&values);
    provider_.UpdateChromePolicy(values);
  }

  virtual void GetQuicAllowedPolicy(PolicyMap* values) = 0;

  // Crashes the network service.
  void CrashNetworkService() {
    SimulateNetworkServiceCrash();
    ConfigureMockCertVerifier();
  }

 private:
  testing::NiceMock<MockConfigurationPolicyProvider> provider_;
};

// Policy QuicAllowed set to false.
class QuicAllowedPolicyIsFalse : public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsFalse() = default;
  QuicAllowedPolicyIsFalse(const QuicAllowedPolicyIsFalse&) = delete;
  QuicAllowedPolicyIsFalse& operator=(const QuicAllowedPolicyIsFalse&) = delete;
  ~QuicAllowedPolicyIsFalse() override = default;

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
    values->Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  }
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
    CrashNetworkService();
    // Make sure the NetworkContext has noticed the pipe was closed.
    g_browser_process->system_network_context_manager()
        ->FlushNetworkInterfaceForTesting();
    EXPECT_FALSE(IsQuicEnabledForSystem());
  }
}

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsFalse,
                       QuicDisallowedForSafeBrowsing) {
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));

  // If using the network service, crash the service, and make sure QUIC is
  // still disabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkService();
    // Make sure the NetworkContext has noticed the pipe was closed.
    g_browser_process->safe_browsing_service()->FlushNetworkInterfaceForTesting(
        browser()->profile());
    EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  }
}

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsFalse, QuicDisallowedForProfile) {
  EXPECT_FALSE(IsQuicEnabled(browser()->profile()));

  // If using the network service, crash the service, and make sure QUIC is
  // still disabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkService();
    // Make sure the NetworkContext has noticed the pipe was closed.
    browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->FlushNetworkInterfaceForTesting();
    EXPECT_FALSE(IsQuicEnabled(browser()->profile()));
  }
}

// Policy QuicAllowed set to true.
class QuicAllowedPolicyIsTrue : public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsTrue() = default;
  QuicAllowedPolicyIsTrue(const QuicAllowedPolicyIsTrue&) = delete;
  QuicAllowedPolicyIsTrue& operator=(const QuicAllowedPolicyIsTrue&) = delete;
  ~QuicAllowedPolicyIsTrue() override = default;

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
    values->Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  }
};

// It's important that all these tests be separate, as the first NetworkContext
// instantiated after the crash could re-disable QUIC globally itself, so can't
// just crash the network service once, and then test all network contexts in
// some particular order.

// TODO(crbug.com/41444868): Flaky on ChromeOS with Network Service
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_QuicAllowedForSystem DISABLED_QuicAllowedForSystem
#else
#define MAYBE_QuicAllowedForSystem QuicAllowedForSystem
#endif
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsTrue, MAYBE_QuicAllowedForSystem) {
  EXPECT_TRUE(IsQuicEnabledForSystem());

  // If using the network service, crash the service, and make sure QUIC is
  // still enabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkService();
    // Make sure the NetworkContext has noticed the pipe was closed.
    g_browser_process->system_network_context_manager()
        ->FlushNetworkInterfaceForTesting();
    EXPECT_TRUE(IsQuicEnabledForSystem());
  }
}

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsTrue, QuicAllowedForSafeBrowsing) {
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing(browser()->profile()));

  // If using the network service, crash the service, and make sure QUIC is
  // still enabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkService();
    // Make sure the NetworkContext has noticed the pipe was closed.
    g_browser_process->safe_browsing_service()->FlushNetworkInterfaceForTesting(
        browser()->profile());
    EXPECT_TRUE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  }
}

// TODO(crbug.com/40777997): Flaky on multiple platforms
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsTrue,
                       DISABLED_QuicAllowedForProfile) {
  EXPECT_TRUE(IsQuicEnabled(browser()->profile()));

  // If using the network service, crash the service, and make sure QUIC is
  // still enabled.
  if (content::IsOutOfProcessNetworkService()) {
    CrashNetworkService();
    // Make sure the NetworkContext has noticed the pipe was closed.
    browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->FlushNetworkInterfaceForTesting();
    EXPECT_TRUE(IsQuicEnabled(browser()->profile()));
  }
}

// Policy QuicAllowed is not set.
class QuicAllowedPolicyIsNotSet : public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsNotSet() = default;
  QuicAllowedPolicyIsNotSet(const QuicAllowedPolicyIsNotSet&) = delete;
  QuicAllowedPolicyIsNotSet& operator=(const QuicAllowedPolicyIsNotSet&) =
      delete;
  ~QuicAllowedPolicyIsNotSet() override = default;

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {}
};

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsNotSet, NoQuicRegulations) {
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_TRUE(IsQuicEnabled(browser()->profile()));
}

// Policy QuicAllowed is set dynamically after profile creation.
// Supports creation of an additional profile.
class QuicAllowedPolicyDynamicTest : public QuicTestBase {
 public:
  QuicAllowedPolicyDynamicTest() : profile_1_(nullptr), profile_2_(nullptr) {}
  QuicAllowedPolicyDynamicTest(const QuicAllowedPolicyDynamicTest&) = delete;
  QuicAllowedPolicyDynamicTest& operator=(const QuicAllowedPolicyDynamicTest&) =
      delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
    // Ensure that QUIC is enabled by default on browser startup.
    command_line->AppendSwitch(switches::kEnableQuic);
    QuicTestBase::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    QuicTestBase::SetUpInProcessBrowserTestFixture();
    // Set the overriden policy provider for the first Profile (profile_1_).
    ON_CALL(policy_for_profile_1_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(policy_for_profile_1_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
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
    ON_CALL(policy_for_profile_2_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(policy_for_profile_2_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::PushProfilePolicyConnectorProviderForTesting(
        &policy_for_profile_2_);

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath path_profile =
        profile_manager->GenerateNextProfileDirectoryPath();
    // Create an additional profile.
    profile_2_ =
        &profiles::testing::CreateProfileSync(profile_manager, path_profile);

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
                   POLICY_SOURCE_CLOUD, base::Value(value), nullptr);
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
  raw_ptr<Profile, DanglingUntriaged> profile_1_;
  // The second profile. Only valid after CreateSecondProfile() has been called.
  raw_ptr<Profile, DanglingUntriaged> profile_2_;

  // Mock Policy for profile_1_.
  MockConfigurationPolicyProvider policy_for_profile_1_;
  // Mock Policy for profile_2_.
  MockConfigurationPolicyProvider policy_for_profile_2_;
};

// QUIC is disallowed by policy after the profile has been initialized.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest, QuicAllowedFalseThenTrue) {
  // After browser start, QuicAllowed=false comes in dynamically
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));

  // Set the QuicAllowed policy to true again
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  // Effectively, QUIC is still disabled because QUIC re-enabling is not
  // supported.
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));

  // Completely remove the QuicAllowed policy
  RemoveAllPolicies(policy_for_profile_1());
  // Effectively, QUIC is still disabled because QUIC re-enabling is not
  // supported.
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));

  // QuicAllowed=false is set again
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
}

// QUIC is allowed, then disallowed by policy after the profile has been
// initialized.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest, QuicAllowedTrueThenFalse) {
  // After browser start, QuicAllowed=true comes in dynamically
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_TRUE(IsQuicEnabled(profile_1()));

  // Completely remove the QuicAllowed policy
  RemoveAllPolicies(policy_for_profile_1());
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_TRUE(IsQuicEnabled(profile_1()));

  // Set the QuicAllowed policy to true again
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_TRUE(IsQuicEnabled(profile_1()));

  // Now set QuicAllowed=false
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
}

// A second Profile is created when QuicAllowed=false policy is in effect for
// the first profile.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest,
                       SecondProfileCreatedWhenQuicAllowedFalse) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled()) {
    return;
  }

  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));

  CreateSecondProfile();

  // QUIC is disabled in both profiles
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
  EXPECT_FALSE(IsQuicEnabled(profile_2()));
}

// A second Profile is created when no QuicAllowed policy is in effect for the
// first profile.
// Then QuicAllowed=false policy is dynamically set for both profiles.
//
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest,
                       QuicAllowedFalseAfterTwoProfilesCreated) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled()) {
    return;
  }

  CreateSecondProfile();

  // QUIC is enabled in both profiles
  EXPECT_TRUE(IsQuicEnabledForSystem());
  EXPECT_TRUE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_TRUE(IsQuicEnabled(profile_1()));
  EXPECT_TRUE(IsQuicEnabled(profile_2()));

  // Disable QUIC in first profile
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
  EXPECT_FALSE(IsQuicEnabled(profile_2()));

  // Disable QUIC in second profile
  SetQuicAllowedPolicy(policy_for_profile_2(), false);
  EXPECT_FALSE(IsQuicEnabledForSystem());
  EXPECT_FALSE(IsQuicEnabledForSafeBrowsing(browser()->profile()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()));
  EXPECT_FALSE(IsQuicEnabled(profile_2()));
}

}  // namespace policy
