// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/enterprise/test/test_constants.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/features.h"
#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

using ManagementContextMixin = enterprise::test::ManagementContextMixin;
using ManagementContext = enterprise::test::ManagementContext;

namespace {

constexpr char kProfileIssuerCommonName[] = "Profile Root CA";
constexpr char kBrowserIssuerCommonName[] = "Browser Root CA";

struct CapturedRequest {
  scoped_refptr<net::X509Certificate> client_certificate{};
};

}  // namespace

class ClientCertificateBrowserTest : public MixinBasedPlatformBrowserTest,
                                     public testing::WithParamInterface<bool> {
 protected:
  ClientCertificateBrowserTest() : scoped_key_converter_(true) {
    management_mixin_ = ManagementContextMixin::Create(
        &mixin_host_, this,
        {
            .is_cloud_user_managed = is_profile_scenario(),
            .is_cloud_machine_managed = !is_profile_scenario(),
            .affiliated = false,
        });

#if BUILDFLAG(IS_ANDROID)
    scoped_feature_list_.InitAndEnableFeature(
        client_certificates::features::
            kEnableClientCertificateProvisioningOnAndroid);
#endif  // BUILDFLAG(IS_ANDROID)
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

  void SetUserPolicy(bool enable_provisioning) {
    base::flat_map<std::string, std::optional<base::Value>> policy_values;
    policy_values.insert(
        {policy::key::kProvisionManagedClientCertificateForUser,
         base::Value(enable_provisioning ? 1 : 0)});

    management_mixin()->SetCloudUserPolicies(std::move(policy_values));
  }

  void SetBrowserPolicy(bool enable_provisioning) {
    base::flat_map<std::string, std::optional<base::Value>> policy_values;
    policy_values.insert(
        {policy::key::kProvisionManagedClientCertificateForBrowser,
         base::Value(enable_provisioning ? 1 : 0)});

    management_mixin()->SetCloudMachinePolicies(std::move(policy_values));
  }

  void SetCertificateAutoselectionPolicy() {
    std::string policy_value_json;
    ASSERT_TRUE(base::JSONWriter::Write(
        base::Value::Dict()
            .Set("pattern", embedded_https_test_server()
                                .GetURL("mtls.google.com", "/mtls")
                                .spec())
            .Set("filter", base::Value::Dict().Set(
                               "ISSUER", base::Value::Dict().Set(
                                             "CN", GetIssuerCommonName()))),
        &policy_value_json));

    base::flat_map<std::string, std::optional<base::Value>> policy_values;
    policy_values.insert(
        {policy::key::kAutoSelectCertificateForUrls,
         base::Value(base::Value::List().Append(policy_value_json))});

    if (is_profile_scenario()) {
      management_mixin()->SetCloudUserPolicies(std::move(policy_values));
    } else {
      management_mixin()->SetCloudMachinePolicies(std::move(policy_values));
    }
  }

  void EnablePolicyAndWaitForIdentity() {
    CertificateProvisioningService* provisioning_service;
    if (is_profile_scenario()) {
      SetUserPolicy(true);
      provisioning_service =
          CertificateProvisioningServiceFactory::GetForProfile(GetProfile());
    } else {
      SetBrowserPolicy(true);
      provisioning_service = g_browser_process->browser_policy_connector()
                                 ->chrome_browser_cloud_management_controller()
                                 ->GetCertificateProvisioningService();
    }

    ASSERT_TRUE(provisioning_service);

    base::test::TestFuture<std::optional<ClientIdentity>> test_future;
    provisioning_service->GetManagedIdentity(test_future.GetCallback());

    VerifyIdentity(test_future.Get());
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

  std::string_view GetIssuerCommonName() {
    return is_profile_scenario() ? kProfileIssuerCommonName
                                 : kBrowserIssuerCommonName;
  }

  ManagementContextMixin* management_mixin() { return management_mixin_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  bool is_profile_scenario() const { return GetParam(); }

  std::optional<CapturedRequest> captured_request_;

 private:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<policy::EmbeddedPolicyTestServer> test_dm_server_;
  client_certificates::ScopedSSLKeyConverter scoped_key_converter_;
  std::unique_ptr<ManagementContextMixin> management_mixin_;
  base::test::ScopedFeatureList scoped_feature_list_;
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

// Temporarily disabled on Android due to PRE_ tests not being fully supported.
// See crbug.com/40200835
#if !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(ClientCertificateBrowserTest, UseIdentityInMtls) {
  // Enable the necessary policies and trigger a navigation.
  SetCertificateAutoselectionPolicy();
  if (is_profile_scenario()) {
    SetUserPolicy(true);
  } else {
    SetBrowserPolicy(true);
  }

  ASSERT_TRUE(content::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this),
      embedded_https_test_server().GetURL("mtls.google.com", "/mtls")));

  ASSERT_TRUE(captured_request_);
  VerifyClientCert(captured_request_->client_certificate);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ClientCertificateBrowserTest,
    testing::Bool());

}  // namespace client_certificates
