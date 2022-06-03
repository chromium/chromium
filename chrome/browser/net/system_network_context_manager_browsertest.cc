// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/first_party_sets_component_installer.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/test_cert_verifier_service_factory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "net/base/features.h"
#endif

namespace {

int64_t GetFirstPartySetEntriesCountFromNetworkService() {
  // The test interface isn't supported in the in-process case.
  DCHECK(!content::IsInProcessNetworkService());

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterface(
      network_service_test.BindNewPipeAndPassReceiver());
  network_service_test.FlushForTesting();

  mojo::ScopedAllowSyncCallForTesting allow_sync_call;

  int64_t count = 0;
  EXPECT_TRUE(network_service_test->GetFirstPartySetEntriesCount(&count));

  return count;
}

void Sleep(base::TimeDelta duration) {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}

// Calls a predicate periodically until it becomes true, or until a timeout
// elapses. The predicate is expected to be monotonic (i.e. once it becomes
// true, it will remain true until the next time `predicate` is invoked).
// Returns true if the predicate became true; false otherwise.
bool PollWithTimeout(base::TimeDelta timeout,
                     base::RepeatingCallback<bool()> predicate) {
  base::TimeTicks deadline = base::TimeTicks::Now() + timeout;
  while (base::TimeTicks::Now() <= deadline) {
    if (predicate.Run())
      return true;
    Sleep(base::Milliseconds(5));
  }
  return false;
}

void PollForFirstPartySetEntryCount(base::TimeDelta timeout,
                                    int64_t expected_count) {
  if (!PollWithTimeout(
          timeout, base::BindLambdaForTesting([expected_count]() {
            return GetFirstPartySetEntriesCountFromNetworkService() ==
                   expected_count;
          }))) {
    FAIL() << "Polled for " << timeout << " but never found exactly "
           << expected_count << " First-Party Set entries.";
  }
}

}  // namespace

using SystemNetworkContextManagerBrowsertest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerBrowsertest,
                       StaticAuthParams) {
  // Test defaults.
  network::mojom::HttpAuthStaticParamsPtr static_params =
      SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting();
  EXPECT_THAT(static_params->supported_schemes,
              testing::ElementsAre("basic", "digest", "ntlm", "negotiate"));
  EXPECT_EQ("", static_params->gssapi_library_name);

  // Test that prefs are reflected in params.

  PrefService* local_state = g_browser_process->local_state();

  local_state->SetString(prefs::kAuthSchemes, "basic");
  static_params =
      SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting();
  EXPECT_THAT(static_params->supported_schemes, testing::ElementsAre("basic"));

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  const char dev_null[] = "/dev/null";
  local_state->SetString(prefs::kGSSAPILibraryName, dev_null);
  static_params =
      SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting();
  EXPECT_EQ(dev_null, static_params->gssapi_library_name);
#endif
}

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerBrowsertest, AuthParams) {
  // Test defaults.
  network::mojom::HttpAuthDynamicParamsPtr dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_FALSE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_FALSE(dynamic_params->enable_negotiate_port);
  EXPECT_TRUE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  PrefService* local_state = g_browser_process->local_state();

  local_state->SetBoolean(prefs::kDisableAuthNegotiateCnameLookup, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_FALSE(dynamic_params->enable_negotiate_port);
  EXPECT_TRUE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  local_state->SetBoolean(prefs::kEnableAuthNegotiatePort, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_TRUE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  local_state->SetBoolean(prefs::kBasicAuthOverHttpEnabled, false);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  const char kServerAllowList[] = "foo";
  local_state->SetString(prefs::kAuthServerAllowlist, kServerAllowList);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);

  const char kDelegateAllowList[] = "bar, baz";
  local_state->SetString(prefs::kAuthNegotiateDelegateAllowlist,
                         kDelegateAllowList);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ(kDelegateAllowList, dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

#if defined(OS_LINUX) || defined(OS_MAC) || defined(OS_CHROMEOS)
  local_state->SetBoolean(prefs::kAuthNegotiateDelegateByKdcPolicy, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_EQ(kDelegateAllowList, dynamic_params->delegate_allowlist);
  EXPECT_TRUE(dynamic_params->delegate_by_kdc_policy);
#endif  // defined(OS_LINUX) || defined(OS_MAC) || defined(OS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The kerberos.enabled pref is false and the device is not Active Directory
  // managed by default.
  EXPECT_FALSE(dynamic_params->allow_gssapi_library_load);
  local_state->SetBoolean(prefs::kKerberosEnabled, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_TRUE(dynamic_params->allow_gssapi_library_load);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

class SystemNetworkContextManagerWithFirstPartySetComponentBrowserTest
    : public SystemNetworkContextManagerBrowsertest {
 public:
  SystemNetworkContextManagerWithFirstPartySetComponentBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    SystemNetworkContextManagerBrowsertest::SetUpInProcessBrowserTestFixture();
    feature_list_.InitAndEnableFeature(net::features::kFirstPartySets);
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::FirstPartySetsComponentInstallerPolicy::
        WriteComponentForTesting(component_dir_.GetPath(), R"([{
          "owner": "https://example.test",
          "members": [
            "https://member1.test",
            "https://member2.test"
            ]
          }])");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir component_dir_;
};

IN_PROC_BROWSER_TEST_F(
    SystemNetworkContextManagerWithFirstPartySetComponentBrowserTest,
    ReloadsFirstPartySetsAfterCrash) {
  // Network service is not running out of process, so cannot be crashed.
  if (!content::IsOutOfProcessNetworkService())
    return;

  PollForFirstPartySetEntryCount(base::Seconds(5),
                                 /*expected_count=*/3);

  SimulateNetworkServiceCrash();

  PollForFirstPartySetEntryCount(base::Seconds(5),
                                 /*expected_count=*/3);
}

class SystemNetworkContextManagerReferrersFeatureBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerReferrersFeatureBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(features::kNoReferrers,
                                              GetParam());
  }
  ~SystemNetworkContextManagerReferrersFeatureBrowsertest() override {}

  void SetUpOnMainThread() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that toggling the kNoReferrers feature correctly changes the default
// value of the kEnableReferrers pref.
IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerReferrersFeatureBrowsertest,
                       TestDefaultReferrerReflectsFeatureValue) {
  ASSERT_TRUE(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  EXPECT_NE(local_state->GetBoolean(prefs::kEnableReferrers), GetParam());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemNetworkContextManagerReferrersFeatureBrowsertest,
                         ::testing::Bool());

class SystemNetworkContextManagerFreezeQUICUaBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerFreezeQUICUaBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(blink::features::kReduceUserAgent,
                                              GetParam());
  }
  ~SystemNetworkContextManagerFreezeQUICUaBrowsertest() override {}

  void SetUpOnMainThread() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerFreezeQUICUaBrowsertest,
                       QUICUaConfig) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();

  std::string quic_ua = network_context_params->quic_user_agent_id;

  if (GetParam()) {  // if the UA Freeze feature is turned on
    EXPECT_EQ("", quic_ua);
  } else {
    EXPECT_THAT(quic_ua, testing::HasSubstr(chrome::GetChannelName(
                             chrome::WithExtendedStable(false))));
    EXPECT_THAT(quic_ua,
                testing::HasSubstr(
                    version_info::GetProductNameAndVersionForUserAgent()));
    EXPECT_THAT(quic_ua, testing::HasSubstr(content::BuildOSCpuInfo(
                             content::IncludeAndroidBuildNumber::Exclude,
                             content::IncludeAndroidModel::Include)));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemNetworkContextManagerFreezeQUICUaBrowsertest,
                         ::testing::Bool());

class SystemNetworkContextManagerWPADQuickCheckBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerWPADQuickCheckBrowsertest() = default;
  ~SystemNetworkContextManagerWPADQuickCheckBrowsertest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerWPADQuickCheckBrowsertest,
                       WPADQuickCheckPref) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kQuickCheckEnabled, GetParam());

  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();
  EXPECT_EQ(GetParam(), network_context_params->pac_quick_check_enabled);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemNetworkContextManagerWPADQuickCheckBrowsertest,
                         ::testing::Bool());

class SystemNetworkContextManagerCertificateTransparencyBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<absl::optional<bool>> {
 public:
  SystemNetworkContextManagerCertificateTransparencyBrowsertest() {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        GetParam());
  }
  ~SystemNetworkContextManagerCertificateTransparencyBrowsertest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        absl::nullopt);
  }
};

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
class SystemNetworkContextServiceCertVerifierBuiltinPermissionsPolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextServiceCertVerifierBuiltinPermissionsPolicyTest() {
    bool use_builtin_cert_verifier = GetParam();
    cert_verifier_impl_ =
        use_builtin_cert_verifier
            ? cert_verifier::mojom::CertVerifierCreationParams::
                  CertVerifierImpl::kBuiltin
            : cert_verifier::mojom::CertVerifierCreationParams::
                  CertVerifierImpl::kSystem;
  }

  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kCertVerifierBuiltinFeature,
        cert_verifier_impl_ ==
            cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl::
                kBuiltin);

    content::SetCertVerifierServiceFactoryForTesting(
        &test_cert_verifier_service_factory_);

    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    content::SetCertVerifierServiceFactoryForTesting(nullptr);
  }

  void SetUpOnMainThread() override {
    test_cert_verifier_service_factory_.ReleaseAllCertVerifierParams();
  }

  void ExpectUseBuiltinCertVerifierCorrect(
      network::mojom::NetworkContextParamsPtr& network_context_params_ptr,
      cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl
          use_builtin_cert_verifier) {
    ASSERT_TRUE(network_context_params_ptr);
    ASSERT_TRUE(network_context_params_ptr->cert_verifier_params);
    ASSERT_EQ(1ul, test_cert_verifier_service_factory_.num_captured_params());
    ASSERT_TRUE(test_cert_verifier_service_factory_.GetParamsAtIndex(0)
                    ->creation_params);
    EXPECT_EQ(use_builtin_cert_verifier,
              test_cert_verifier_service_factory_.GetParamsAtIndex(0)
                  ->creation_params->use_builtin_cert_verifier);
    // Send it to the actual CertVerifierServiceFactory.
    test_cert_verifier_service_factory_.ReleaseNextCertVerifierParams();
  }

  cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl
  cert_verifier_impl() const {
    return cert_verifier_impl_;
  }

 private:
  cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl
      cert_verifier_impl_;
  base::test::ScopedFeatureList scoped_feature_list_;

  cert_verifier::TestCertVerifierServiceFactoryImpl
      test_cert_verifier_service_factory_;
};

IN_PROC_BROWSER_TEST_P(
    SystemNetworkContextServiceCertVerifierBuiltinPermissionsPolicyTest,
    Test) {
  network::mojom::NetworkContextParamsPtr network_context_params_ptr;

  // If no BuiltinCertificateVerifierEnabled policy is set, the
  // use_builtin_cert_verifier param should be set from the feature flag.
  network_context_params_ptr =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();
  ExpectUseBuiltinCertVerifierCorrect(network_context_params_ptr,
                                      cert_verifier_impl());
#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
  // If the BuiltinCertificateVerifierEnabled policy is set it should
  // override the feature flag.
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kBuiltinCertificateVerifierEnabled,
            base::Value(true));
  UpdateProviderPolicy(policies);

  network_context_params_ptr =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();
  ExpectUseBuiltinCertVerifierCorrect(
      network_context_params_ptr,
      cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl::
          kBuiltin);

  SetPolicy(&policies, policy::key::kBuiltinCertificateVerifierEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);

  network_context_params_ptr =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();
  ExpectUseBuiltinCertVerifierCorrect(
      network_context_params_ptr,
      cert_verifier::mojom::CertVerifierCreationParams::CertVerifierImpl::
          kSystem);
#endif  // BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemNetworkContextServiceCertVerifierBuiltinPermissionsPolicyTest,
    ::testing::Bool());
#endif  // BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
