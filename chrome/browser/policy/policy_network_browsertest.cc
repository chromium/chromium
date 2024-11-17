// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/util.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_doh_server.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/gurl.h"


namespace policy {

class SSLPolicyTest : public PolicyTest {
 public:
  SSLPolicyTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

 protected:
  struct LoadResult {
    bool success;
    std::u16string title;
  };

  bool StartTestServer(const net::SSLServerConfig& ssl_config) {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    return https_server_.Start();
  }

  bool StartTestServer(
      const net::EmbeddedTestServer::ServerCertificateConfig& cert_config,
      const net::SSLServerConfig& ssl_config) {
    https_server_.SetSSLConfig(cert_config, ssl_config);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    return https_server_.Start();
  }

  bool GetBooleanPref(const std::string& pref_name) {
    return g_browser_process->local_state()->GetBoolean(pref_name);
  }

  std::optional<bool> GetManagedBooleanPref(const std::string& pref_name) {
    if (g_browser_process->local_state()->IsManagedPreference(pref_name)) {
      return GetBooleanPref(pref_name);
    }
    return std::nullopt;
  }

  LoadResult LoadPage(std::string_view path) {
    return LoadPage(https_server_.GetURL(path));
  }

  LoadResult LoadPage(const GURL& url) {
    EXPECT_TRUE(NavigateToUrl(url, this));
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    if (web_contents->GetController().GetLastCommittedEntry()->GetPageType() ==
        content::PAGE_TYPE_ERROR) {
      return LoadResult{false, u""};
    }
    return LoadResult{true, web_contents->GetTitle()};
  }

  void ExpectVersionOrCipherMismatch() {
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    EXPECT_TRUE(content::EvalJs(web_contents,
                                "document.body.innerHTML.indexOf('ERR_SSL_"
                                "VERSION_OR_CIPHER_MISMATCH') >= 0")
                    .ExtractBool());
  }

 private:
  net::EmbeddedTestServer https_server_;
};

class PostQuantumPolicyTest : public SSLPolicyTest {
 public:
  PostQuantumPolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(net::features::kPostQuantumKyber);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PostQuantumPolicyTest, PostQuantumEnabledPolicy) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_X25519MLKEM768};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // Should be able to load a page from the test server because policy is in
  // the default state and feature is enabled.
  EXPECT_EQ(GetManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
            std::nullopt);
  LoadResult result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  // Disable the policy.
  PolicyMap policies;
  SetPolicy(&policies, key::kPostQuantumKeyAgreementEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // Page loads should now fail.
  EXPECT_EQ(GetManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
            false);
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);

  // Enable the policy.
  PolicyMap policies2;
  SetPolicy(&policies2, key::kPostQuantumKeyAgreementEnabled,
            base::Value(true));
  UpdateProviderPolicy(policies2);
  content::FlushNetworkServiceInstanceForTesting();

  // Page load should now succeed.
  EXPECT_EQ(GetManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
            true);
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PostQuantumPolicyTest, DevicePostQuantumEnabledPolicy) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_X25519MLKEM768};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // Should be able to load a page from the test server because policy is in
  // the default state and feature is enabled.
  EXPECT_EQ(GetManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
            std::nullopt);
  EXPECT_EQ(GetManagedBooleanPref(prefs::kDevicePostQuantumKeyAgreementEnabled),
            std::nullopt);
  LoadResult result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  {
    // Disable the device policy.
    PolicyMap policies;
    SetPolicy(&policies, key::kDevicePostQuantumKeyAgreementEnabled,
              base::Value(false));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page loads should now fail.
  EXPECT_EQ(GetManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
            std::nullopt);
  EXPECT_EQ(GetManagedBooleanPref(prefs::kDevicePostQuantumKeyAgreementEnabled),
            false);
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);

  {
    // Try enabling the non-device policy, while the device policy is disabled.
    PolicyMap policies;
    SetPolicy(&policies, key::kPostQuantumKeyAgreementEnabled,
              base::Value(true));
    SetPolicy(&policies, key::kDevicePostQuantumKeyAgreementEnabled,
              base::Value(false));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page loads should still fail since the device policy takes precedence.
  EXPECT_EQ(GetManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
            true);
  EXPECT_EQ(GetManagedBooleanPref(prefs::kDevicePostQuantumKeyAgreementEnabled),
            false);
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);

  {
    // Enable the device policy.
    PolicyMap policies;
    SetPolicy(&policies, key::kDevicePostQuantumKeyAgreementEnabled,
              base::Value(true));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now succeed.
  EXPECT_EQ(GetManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
            std::nullopt);
  EXPECT_EQ(GetManagedBooleanPref(prefs::kDevicePostQuantumKeyAgreementEnabled),
            true);
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  {
    // Try disabling the non-device policy, while the device policy is enabled.
    PolicyMap policies;
    SetPolicy(&policies, key::kPostQuantumKeyAgreementEnabled,
              base::Value(false));
    SetPolicy(&policies, key::kDevicePostQuantumKeyAgreementEnabled,
              base::Value(true));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should still succeed since the device policy takes precedence.
  EXPECT_EQ(GetManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
            false);
  EXPECT_EQ(GetManagedBooleanPref(prefs::kDevicePostQuantumKeyAgreementEnabled),
            true);
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class ECHPolicyTest : public SSLPolicyTest {
 public:
  // a.test is covered by `CERT_TEST_NAMES`.
  static constexpr std::string_view kHostname = "a.test";
  static constexpr std::string_view kPublicName = "public-name.test";
  static constexpr std::string_view kDohServerHostname = "doh.test";

  static constexpr std::string_view kECHSuccessTitle = "Negotiated ECH";
  static constexpr std::string_view kECHFailureTitle = "Did not negotiate ECH";

  ECHPolicyTest() : ech_server_{net::EmbeddedTestServer::TYPE_HTTPS} {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{net::features::kUseDnsHttpsSvcb,
          {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"}}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    // Configure `ech_server_` to enable and require ECH.
    net::SSLServerConfig server_config;
    std::vector<uint8_t> ech_config_list;
    server_config.ech_keys = net::MakeTestEchKeys(
        kPublicName, /*max_name_len=*/64, &ech_config_list);
    ASSERT_TRUE(server_config.ech_keys);
    ech_server_.RegisterRequestHandler(
        base::BindRepeating(&ECHPolicyTest::HandleRequest));
    ech_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                             server_config);

    ASSERT_TRUE(ech_server_.Start());

    // Start a DoH server, which ensures we use a resolver with HTTPS RR
    // support. Configure it to serve records for `ech_server_`.
    doh_server_.SetHostname(kDohServerHostname);
    url::SchemeHostPort ech_host(GetURL("/"));
    doh_server_.AddAddressRecord(ech_host.host(),
                                 net::IPAddress::IPv4Localhost());
    doh_server_.AddRecord(net::BuildTestHttpsServiceRecord(
        net::dns_util::GetNameForHttpsQuery(ech_host),
        /*priority=*/1, /*service_name=*/ech_host.host(),
        {net::BuildTestHttpsServiceEchConfigParam(ech_config_list)}));
    ASSERT_TRUE(doh_server_.Start());

    // Add a single bootstrapping rule so we can resolve the DoH server.
    host_resolver()->AddRule(kDohServerHostname, "127.0.0.1");

    // The net stack doesn't enable DoH when it can't find a system DNS config
    // (see https://crbug.com/1251715).
    SetReplaceSystemDnsConfig();

    // Via policy, configure the network service to use `doh_server_`.
    UpdateProviderPolicy(PolicyMapWithDohServer());
    content::FlushNetworkServiceInstanceForTesting();
  }

  PolicyMap PolicyMapWithDohServer() {
    PolicyMap policies;
    SetPolicy(&policies, key::kDnsOverHttpsMode, base::Value("secure"));
    SetPolicy(&policies, key::kDnsOverHttpsTemplates,
              base::Value(doh_server_.GetTemplate()));

// On Lacros, the DnsOverHttpsTemplates policy gets mapped to the
// kDnsOverHttpsEffectiveTemplatesChromeOS pref which is set in Ash.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    g_browser_process->local_state()->SetString(
        prefs::kDnsOverHttpsEffectiveTemplatesChromeOS,
        doh_server_.GetTemplate());
#endif
    return policies;
  }

  GURL GetURL(std::string_view path) {
    return ech_server_.GetURL(kHostname, path);
  }

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("text/html; charset=utf-8");
    if (request.ssl_info->encrypted_client_hello) {
      response->set_content(
          base::StrCat({"<title>", kECHSuccessTitle, "</title>"}));
    } else {
      response->set_content(
          base::StrCat({"<title>", kECHFailureTitle, "</title>"}));
    }
    return response;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::TestDohServer doh_server_;
  net::EmbeddedTestServer ech_server_;
};

IN_PROC_BROWSER_TEST_F(ECHPolicyTest, ECHEnabledPolicy) {
  // By default, the policy does not inhibit ECH.
  EXPECT_TRUE(GetBooleanPref(prefs::kEncryptedClientHelloEnabled));
  LoadResult result = LoadPage(GetURL("/a"));
  EXPECT_TRUE(result.success);
  EXPECT_EQ(base::ASCIIToUTF16(kECHSuccessTitle), result.title);

  // Disable the policy.
  PolicyMap policies = PolicyMapWithDohServer();
  SetPolicy(&policies, key::kEncryptedClientHelloEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // ECH should no longer be enabled.
  EXPECT_FALSE(GetBooleanPref(prefs::kEncryptedClientHelloEnabled));
  result = LoadPage(GetURL("/b"));
  EXPECT_TRUE(result.success);
  EXPECT_EQ(base::ASCIIToUTF16(kECHFailureTitle), result.title);
}

}  // namespace policy
