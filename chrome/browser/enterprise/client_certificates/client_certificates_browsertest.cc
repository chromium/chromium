// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <vector>

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/enterprise/test/test_constants.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/context_delegate.h"
#include "components/enterprise/client_certificates/core/features.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/enterprise/client_certificates/android/browser_binding/browser_key.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_store.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace client_certificates {

using ManagementContextMixin = enterprise::test::ManagementContextMixin;
using ManagementContext = enterprise::test::ManagementContext;

namespace {

constexpr char kProfileIssuerCommonName[] = "Profile Root CA";
constexpr char kBrowserIssuerCommonName[] = "Browser Root CA";

struct CapturedRequest {
  scoped_refptr<net::X509Certificate> client_certificate{};
};

#if BUILDFLAG(IS_ANDROID)
constexpr char kKeyLoadResultHistogram[] =
    "Enterprise.ClientCertificates.ClankKeyLoadResult";
// ClankClientCertificatesKeyLoadResult value "Not found" in
// tools/metrics/histograms/metadata/enterprise/enums.xml; histogram enum
// values are stable.
constexpr int kKeyLoadResultNotFound = 1;
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

class ClientCertificateBrowserTest : public MixinBasedPlatformBrowserTest,
                                     public testing::WithParamInterface<bool> {
 protected:
  ClientCertificateBrowserTest()
      : ClientCertificateBrowserTest(/*mock_unexportable_keys=*/true) {}

  // When `mock_unexportable_keys` is false, no unexportable key provider is
  // available and the next key source is used for the managed identity's
  // private key. On Android that is the keystore-backed
  // `AndroidPrivateKeyFactory`, as in production, where //crypto has no
  // unexportable key provider.
  explicit ClientCertificateBrowserTest(bool mock_unexportable_keys)
      : scoped_key_converter_(mock_unexportable_keys) {
    scoped_feature_list_.InitWithFeatures(
        {::features::kLazyKeyedServiceInstantiation,
         features::kClientCertCommonNameAttribute},
        {});
    management_mixin_ = ManagementContextMixin::Create(
        &mixin_host_, this,
        {
            .is_cloud_user_managed = is_profile_scenario(),
            .is_cloud_machine_managed = !is_profile_scenario(),
            .affiliated = false,
        });
  }

  void SetUp() override {
    embedded_https_test_server().SetCertHostnames({});
    net::EmbeddedTestServer::ServerCertificateConfig server_cert_config;
    server_cert_config.dns_names = {"mtls.google.com"};
    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    embedded_https_test_server().SetSSLConfig(server_cert_config, ssl_config);

    CHECK(embedded_https_test_server().InitializeAndListen());
    MixinBasedPlatformBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    test_dm_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();

    if (!is_profile_scenario()) {
      test_dm_server_->client_storage()->RegisterClient(
          enterprise::test::CreateBrowserClientInfo());
    }

    ASSERT_TRUE(test_dm_server_->Start());

    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    test_dm_server_->GetServiceURL().spec());

    MixinBasedPlatformBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void PreRunTestOnMainThread() override {
    // At this point ProfileImpl and its ServiceIsCreatedWithBrowserContext()
    // services have been created, while
    // InProcessBrowserTest::PreRunTestOnMainThread() has not yet marked startup
    // as complete via AfterStartupTaskUtils.
    Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
    ASSERT_TRUE(profile);
    const KeyedServiceBaseFactory* factory =
        CertificateProvisioningServiceFactory::GetInstance();
    was_profile_service_created_before_startup_complete_ =
        factory->IsServiceCreated(profile);

    MixinBasedPlatformBrowserTest::PreRunTestOnMainThread();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &ClientCertificateBrowserTest::HandleRequest, base::Unretained(this)));

    embedded_https_test_server().StartAcceptingConnections();

    MixinBasedPlatformBrowserTest::SetUpOnMainThread();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const auto& request_url = request.GetURL();
    if (request_url.path() != "/mtls") {
      SCOPED_TRACE("Unexpected request.");
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (!request.ssl_info || !request.ssl_info->cert) {
      response->set_code(net::HTTP_FORBIDDEN);
      return response;
    }

    captured_request_.emplace();
    captured_request_->client_certificate = request.ssl_info->cert;
    return response;
  }

  void SetUserPolicy(
      int policy_value,
      std::optional<std::string> common_name_source = std::nullopt) {
    base::flat_map<std::string, std::optional<base::Value>> policy_values;
    policy_values.insert(
        {policy::key::kProvisionManagedClientCertificateForUser,
         base::Value(policy_value)});
    if (common_name_source.has_value()) {
      policy_values.insert({policy::key::kManagedClientCertificateForUserConfig,
                            base::Value(base::DictValue().Set(
                                kCommonNameSourceKey, *common_name_source))});
    }

    management_mixin()->SetCloudUserPolicies(std::move(policy_values));
  }

  void SetBrowserPolicy(
      int policy_value,
      std::optional<std::string> common_name_source = std::nullopt) {
    base::flat_map<std::string, std::optional<base::Value>> policy_values;
    policy_values.insert(
        {policy::key::kProvisionManagedClientCertificateForBrowser,
         base::Value(policy_value)});
    if (common_name_source.has_value()) {
      policy_values.insert(
          {policy::key::kManagedClientCertificateForBrowserConfig,
           base::Value(base::DictValue().Set(kCommonNameSourceKey,
                                             *common_name_source))});
    }

    management_mixin()->SetCloudMachinePolicies(std::move(policy_values));
  }

  void SetCertificateAutoselectionPolicy() {
    std::string policy_value_json;
    ASSERT_TRUE(base::JSONWriter::Write(
        base::DictValue()
            .Set("pattern", embedded_https_test_server()
                                .GetURL("mtls.google.com", "/mtls")
                                .spec())
            .Set("filter", base::DictValue().Set(
                               "ISSUER", base::DictValue().Set(
                                             "CN", GetIssuerCommonName()))),
        &policy_value_json));

    base::flat_map<std::string, std::optional<base::Value>> policy_values;
    policy_values.insert(
        {policy::key::kAutoSelectCertificateForUrls,
         base::Value(base::ListValue().Append(policy_value_json))});

    if (is_profile_scenario()) {
      management_mixin()->SetCloudUserPolicies(std::move(policy_values));
    } else {
      management_mixin()->SetCloudMachinePolicies(std::move(policy_values));
    }
  }

  CertificateProvisioningService* GetProvisioningService() {
    if (is_profile_scenario()) {
      return CertificateProvisioningServiceFactory::GetForProfile(GetProfile());
    }
    return g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->GetCertificateProvisioningService();
  }

  std::optional<ClientIdentity> GetManagedIdentity() {
    CertificateProvisioningService* provisioning_service =
        GetProvisioningService();
    if (!provisioning_service) {
      return std::nullopt;
    }
    base::test::TestFuture<std::optional<ClientIdentity>> test_future;
    provisioning_service->GetManagedIdentity(test_future.GetCallback());
    return test_future.Get();
  }

  void EnablePolicyAndWaitForIdentity(
      std::optional<std::string> common_name_source = std::nullopt) {
    if (is_profile_scenario()) {
      SetUserPolicy(1, std::move(common_name_source));
    } else {
      SetBrowserPolicy(1, std::move(common_name_source));
    }

    ASSERT_TRUE(GetProvisioningService());
    VerifyIdentity(GetManagedIdentity());
  }

  void VerifyIdentity(const std::optional<ClientIdentity>& managed_identity) {
    ASSERT_TRUE(managed_identity);
    ASSERT_TRUE(managed_identity->is_valid());
    VerifyClientCert(managed_identity->certificate);
  }

  void VerifyClientCert(
      const scoped_refptr<net::X509Certificate>& client_certificate) {
    // Verify that the right cert was created based on the root CA's CN.
    ASSERT_TRUE(client_certificate);
    EXPECT_EQ(client_certificate->issuer().common_name, GetIssuerCommonName());
  }

  std::string GetDefaultExpectedCommonName() {
    if (is_profile_scenario()) {
      return enterprise::ProfileIdServiceFactory::GetForProfile(GetProfile())
          ->GetProfileId()
          .value_or(std::string());
    }
    return policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
  }

  std::string_view GetIssuerCommonName() {
    return is_profile_scenario() ? kProfileIssuerCommonName
                                 : kBrowserIssuerCommonName;
  }

  ManagementContextMixin* management_mixin() { return management_mixin_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  bool is_profile_scenario() const { return GetParam(); }

  bool was_profile_service_created_before_startup_complete_ = true;
  std::optional<CapturedRequest> captured_request_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<policy::EmbeddedPolicyTestServer> test_dm_server_;
  client_certificates::ScopedSSLKeyConverter scoped_key_converter_;
  std::unique_ptr<ManagementContextMixin> management_mixin_;
};

IN_PROC_BROWSER_TEST_P(ClientCertificateBrowserTest, CreateNewIdentity) {
  EnablePolicyAndWaitForIdentity();
  histogram_tester().ExpectUniqueSample(
      base::StringPrintf(
          "Enterprise.ClientCertificate.%s.Provisioning.CertificateCreation."
          "Outcome",
          is_profile_scenario() ? "Profile" : "Browser"),
      true, 1);
}

IN_PROC_BROWSER_TEST_P(ClientCertificateBrowserTest,
                       CreateNewIdentityWithComputerName) {
  EnablePolicyAndWaitForIdentity(kCommonNameSourceComputerName);
  std::string expected_device_name =
      SanitizeAndTruncateCommonName(policy::GetDeviceName());
  std::optional<ClientIdentity> identity = GetManagedIdentity();
  ASSERT_TRUE(identity);
  EXPECT_EQ(identity->certificate->subject().common_name,
            expected_device_name.empty() ? GetDefaultExpectedCommonName()
                                         : expected_device_name);
}

IN_PROC_BROWSER_TEST_P(ClientCertificateBrowserTest, PRE_LoadExistingIdentity) {
  EnablePolicyAndWaitForIdentity();
}

IN_PROC_BROWSER_TEST_P(ClientCertificateBrowserTest, LoadExistingIdentity) {
  EnablePolicyAndWaitForIdentity();
  histogram_tester().ExpectUniqueSample(
      base::StringPrintf(
          "Enterprise.ClientCertificate.%s.Provisioning.ExistingIdentity."
          "Outcome",
          is_profile_scenario() ? "Profile" : "Browser"),
      true, 1);
}

IN_PROC_BROWSER_TEST_P(ClientCertificateBrowserTest, UseIdentityInMtls) {
  // Enable the necessary policies and trigger a navigation.
  SetCertificateAutoselectionPolicy();
  if (is_profile_scenario()) {
    SetUserPolicy(1);
  } else {
    SetBrowserPolicy(1);
  }

  ASSERT_TRUE(content::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this),
      embedded_https_test_server().GetURL("mtls.google.com", "/mtls")));

  ASSERT_TRUE(captured_request_);
  VerifyClientCert(captured_request_->client_certificate);
}

// The profile-scoped provisioning service is not needed during startup, so it
// is created once browser startup is complete rather than with the profile.
IN_PROC_BROWSER_TEST_P(ClientCertificateBrowserTest,
                       ProfileServiceIsCreatedAfterStartup) {
  EXPECT_FALSE(was_profile_service_created_before_startup_complete_);

  // Tasks posted through AfterStartupTaskUtils at BEST_EFFORT priority run in
  // order, so by the time this one runs the profile's post-startup
  // KeyedServices have been created.
  base::RunLoop run_loop;
  AfterStartupTaskUtils::PostTask(
      FROM_HERE,
      content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT}),
      run_loop.QuitClosure());
  run_loop.Run();

  // IsServiceCreated() is public on KeyedServiceBaseFactory but protected on
  // the templated subclasses, so go through the base pointer.
  const KeyedServiceBaseFactory* factory =
      CertificateProvisioningServiceFactory::GetInstance();
  EXPECT_TRUE(factory->IsServiceCreated(GetProfile()));
}

IN_PROC_BROWSER_TEST_P(ClientCertificateBrowserTest,
                       UseIdentityInMtlsWithComputerName) {
  // Enable the necessary policies and trigger a navigation.
  SetCertificateAutoselectionPolicy();
  if (is_profile_scenario()) {
    SetUserPolicy(1, kCommonNameSourceComputerName);
  } else {
    SetBrowserPolicy(1, kCommonNameSourceComputerName);
  }

  ASSERT_TRUE(content::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this),
      embedded_https_test_server().GetURL("mtls.google.com", "/mtls")));

  ASSERT_TRUE(captured_request_);
  VerifyClientCert(captured_request_->client_certificate);
  std::string expected_device_name =
      SanitizeAndTruncateCommonName(policy::GetDeviceName());
  EXPECT_EQ(captured_request_->client_certificate->subject().common_name,
            expected_device_name.empty() ? GetDefaultExpectedCommonName()
                                         : expected_device_name);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ClientCertificateBrowserTest,
    testing::Bool());

#if BUILDFLAG(IS_ANDROID)

// Stores the managed identity's private key in the Android keystore, as in
// production, instead of the mocked unexportable key provider.
class ClientCertificateAndroidKeyStoreBrowserTest
    : public ClientCertificateBrowserTest {
 protected:
  ClientCertificateAndroidKeyStoreBrowserTest()
      : ClientCertificateBrowserTest(/*mock_unexportable_keys=*/false) {}

  void TearDownOnMainThread() override {
    // Keystore entries outlive the test's user data dir; do not leak the
    // provisioned key on the device.
    if (std::optional<ClientIdentity> identity = GetManagedIdentity()) {
      DeleteKeyStoreEntry(*identity);
    }
    ClientCertificateBrowserTest::TearDownOnMainThread();
  }

  // Deletes the keystore entry backing `identity`'s private key while leaving
  // the stored identity (key identifier and certificate) in place, as happens
  // when the keystore loses the key, e.g. after a keystore reset.
  void DeleteKeyStoreEntry(const ClientIdentity& identity) {
    ASSERT_TRUE(identity.private_key);
    ASSERT_EQ(identity.private_key->GetSource(), PrivateKeySource::kAndroidKey);
    const std::string key_id = identity.private_key->ToProto().wrapped_key();
    scoped_refptr<BrowserKeyStore> key_store = CreateBrowserKeyStoreInstance();
    std::vector<uint8_t> credential_id(key_id.begin(), key_id.end());
    key_store->DeleteBrowserKey(credential_id);
    ASSERT_FALSE(key_store->GetBrowserKeyForCredentialId(credential_id));
  }
};

IN_PROC_BROWSER_TEST_P(ClientCertificateAndroidKeyStoreBrowserTest,
                       PRE_RecoversFromLostKeyStoreKey) {
  EnablePolicyAndWaitForIdentity();
  // A fresh user data dir has no stored identity, so no key load is attempted.
  histogram_tester().ExpectTotalCount(kKeyLoadResultHistogram, 0);

  // Lose the key; the identity in storage is left behind.
  // `TearDownOnMainThread()` deletes the entry again, which is a no-op.
  std::optional<ClientIdentity> identity = GetManagedIdentity();
  ASSERT_TRUE(identity);
  DeleteKeyStoreEntry(*identity);
}

// The stored identity refers to a key that the keystore no longer has. Loading
// it must fail so that a new identity is provisioned, rather than a new key
// being paired with the old certificate.
IN_PROC_BROWSER_TEST_P(ClientCertificateAndroidKeyStoreBrowserTest,
                       RecoversFromLostKeyStoreKey) {
  SetCertificateAutoselectionPolicy();
  EnablePolicyAndWaitForIdentity();

  histogram_tester().ExpectUniqueSample(kKeyLoadResultHistogram,
                                        kKeyLoadResultNotFound, 1);
  histogram_tester().ExpectUniqueSample(
      base::StringPrintf(
          "Enterprise.ClientCertificate.%s.Provisioning.CertificateCreation."
          "Outcome",
          is_profile_scenario() ? "Profile" : "Browser"),
      true, 1);

  // The new identity is usable for mTLS.
  ASSERT_TRUE(content::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this),
      embedded_https_test_server().GetURL("mtls.google.com", "/mtls")));
  ASSERT_TRUE(captured_request_);
  VerifyClientCert(captured_request_->client_certificate);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ClientCertificateAndroidKeyStoreBrowserTest,
    testing::Bool());

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace client_certificates
