// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
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
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_doh_server.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
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

  bool StartTestServer(const net::SSLServerConfig ssl_config) {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    return https_server_.Start();
  }

  bool GetBooleanPref(const std::string& pref_name) {
    return g_browser_process->local_state()->GetBoolean(pref_name);
  }

  LoadResult LoadPage(base::StringPiece path) {
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
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPostQuantumKyber);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(PostQuantumPolicyTest, ChromeVariations) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_X25519Kyber768};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // Should be able to load a page from the test server because Kyber is
  // enabled.
  EXPECT_TRUE(GetBooleanPref(prefs::kPostQuantumEnabled));
  LoadResult result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  // Setting ChromeVariations to a non-zero value should also disable
  // Kyber.
  const auto* const variations_key =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // On Chrome OS the ChromeVariations policy doesn't exist and is
      // replaced by DeviceChromeVariations.
      key::kDeviceChromeVariations;
#else
      key::kChromeVariations;
#endif
  PolicyMap policies;
  SetPolicy(&policies, variations_key, base::Value(1));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // Page loads should now fail.
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

class ECHPolicyTest : public SSLPolicyTest {
 public:
  // a.test is covered by `CERT_TEST_NAMES`.
  static constexpr base::StringPiece kHostname = "a.test";
  static constexpr base::StringPiece kPublicName = "public-name.test";
  static constexpr base::StringPiece kDohServerHostname = "doh.test";

  static constexpr base::StringPiece kECHSuccessTitle = "Negotiated ECH";
  static constexpr base::StringPiece kECHFailureTitle = "Did not negotiate ECH";

  ECHPolicyTest() : ech_server_{net::EmbeddedTestServer::TYPE_HTTPS} {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{net::features::kEncryptedClientHello, {}},
         {net::features::kUseDnsHttpsSvcb,
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
    return policies;
  }

  GURL GetURL(base::StringPiece path) {
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

class SHA1DisabledPolicyTest : public SSLPolicyTest {
 public:
  SHA1DisabledPolicyTest() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kSHA1ServerSignature);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SHA1DisabledPolicyTest, InsecureHashPolicy) {
  net::SSLServerConfig ssl_config;
  // Apply 0x303 to force TLS 1.2 and make the server limited to sha1.
  ssl_config.version_min = 0x0303;
  ssl_config.version_max = 0x0303;
  ssl_config.signature_algorithm_for_testing = 0x0201;
  ASSERT_TRUE(StartTestServer(ssl_config));

  // Should be unable to load a page from the test server because the
  // policy is unset, and the feature flag has disabled SHA1
  EXPECT_FALSE(GetBooleanPref(prefs::kInsecureHashesInTLSHandshakesEnabled));
  LoadResult result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);

  PolicyMap policies;
  // Enable Insecure Handshake Hashes.
  SetPolicy(&policies, key::kInsecureHashesInTLSHandshakesEnabled,
            base::Value(true));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // Should be able to load a page from the test server because policy has
  // overridden the disabled feature flag.
  EXPECT_TRUE(GetBooleanPref(prefs::kInsecureHashesInTLSHandshakesEnabled));
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  // Disable the policy.
  SetPolicy(&policies, key::kInsecureHashesInTLSHandshakesEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // Page loads should now fail as the policy has disabled SHA1.
  EXPECT_FALSE(GetBooleanPref(prefs::kInsecureHashesInTLSHandshakesEnabled));
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);
}

class SHA1EnabledPolicyTest : public SSLPolicyTest {
 public:
  SHA1EnabledPolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kSHA1ServerSignature);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SHA1EnabledPolicyTest, InsecureHashPolicy) {
  net::SSLServerConfig ssl_config;
  // Apply 0x303 to force TLS 1.2 and make the server limited to sha1.
  ssl_config.version_min = 0x0303;
  ssl_config.version_max = 0x0303;
  ssl_config.signature_algorithm_for_testing = 0x0201;
  ASSERT_TRUE(StartTestServer(ssl_config));

  // With the policy unset, we should be able to load a page from the test
  // server because SHA1 is allowed by feature flag.
  EXPECT_FALSE(GetBooleanPref(prefs::kInsecureHashesInTLSHandshakesEnabled));
  LoadResult result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  PolicyMap policies;
  // Disable Insecure Handshake Hashes.
  SetPolicy(&policies, key::kInsecureHashesInTLSHandshakesEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // We should no longer be able to load a page, as the policy has
  // disabled insecure hashes..
  EXPECT_FALSE(GetBooleanPref(prefs::kInsecureHashesInTLSHandshakesEnabled));
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);

  // Enable Insecure Handshake Hashes.
  SetPolicy(&policies, key::kInsecureHashesInTLSHandshakesEnabled,
            base::Value(true));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // With the policy set, we should be able to load a page from the test server
  EXPECT_TRUE(GetBooleanPref(prefs::kInsecureHashesInTLSHandshakesEnabled));
  result = LoadPage("/title3.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of More Awesomeness", result.title);
}

}  // namespace policy
