// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/test/test_future.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/server_certificate_database.h"     // nogncheck
#include "chrome/browser/net/server_certificate_database.pb.h"  // nogncheck
#include "chrome/browser/net/server_certificate_database_service.h"  // nogncheck
#include "chrome/browser/net/server_certificate_database_service_factory.h"  // nogncheck
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "crypto/sha2.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/cert/cert_type.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
class CertVerifierServiceChromeRootStoreOptionalTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    // This test puts a test cert in the Chrome Root Store, which will fail in
    // builds where Certificate Transparency is required, so disable CT
    // during this test.
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        false);

    host_resolver()->AddRule("*", "127.0.0.1");

    content::GetCertVerifierServiceFactory()->SetUseChromeRootStore(
        use_chrome_root_store(), base::DoNothing());
  }

  void TearDownOnMainThread() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
    // Reset to default.
    content::GetCertVerifierServiceFactory()->SetUseChromeRootStore(
        true, base::DoNothing());
  }

  bool use_chrome_root_store() const { return GetParam(); }

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceChromeRootStoreOptionalTest, Test) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  // Use a runtime generated cert, as the pre-generated ok_cert has too long of
  // a validity period to be accepted by a publicly trusted root.
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  // The test uses a certificate with a publicly resolvable name, since Chrome
  // rejects certificates for non-unique names from publicly trusted CAs.
  https_test_server.SetCertHostnames({"example.com"});
  ASSERT_TRUE(https_test_server.Start());

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store.
  net::TestRootCerts::GetInstance()->Clear();

  {
    // Create updated Chrome Root Store with just the test server root cert.
    chrome_root_store::RootStore root_store;
    root_store.set_version_major(net::CompiledChromeRootStoreVersion() + 1);

    chrome_root_store::TrustAnchor* anchor = root_store.add_trust_anchors();
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));

    base::RunLoop update_run_loop;
    content::GetCertVerifierServiceFactory()->UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  EXPECT_EQ(use_chrome_root_store(),
            content::NavigateToURL(
                GetActiveWebContents(),
                https_test_server.GetURL("example.com", "/simple.html")));

  // The navigation should show an interstitial if CRS was not in use, since
  // the root was only trusted in the test CRS update and won't be trusted by
  // the platform roots that are used when CRS is not used.
  EXPECT_NE(use_chrome_root_store(),
            chrome_browser_interstitials::IsShowingInterstitial(
                GetActiveWebContents()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifierServiceChromeRootStoreOptionalTest,
                         ::testing::Bool());
#endif  // BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
class CertVerifierTestCrsConstraintsSwitchTest : public PlatformBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    net::EmbeddedTestServer::ServerCertificateConfig test_cert_config;
    test_cert_config.dns_names = {"example.com"};
    test_cert_config.root = net::EmbeddedTestServer::RootType::kUniqueRoot;
    test_server1_.SetSSLConfig(test_cert_config);
    test_server2_.SetSSLConfig(test_cert_config);
    ASSERT_TRUE(test_server1_.InitializeAndListen());
    ASSERT_TRUE(test_server2_.InitializeAndListen());

    scoped_test_root_ =
        net::ScopedTestRoot({test_server1_.GetRoot(), test_server2_.GetRoot()});

    const std::array<uint8_t, crypto::kSHA256Length> root2_hash =
        crypto::SHA256Hash(test_server2_.GetRoot()->cert_span());
    const std::string switch_value =
        base::HexEncode(root2_hash) + ":maxversionexclusive=0";

    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitchASCII(
        net::TrustStoreChrome::kTestCrsConstraintsSwitch, switch_value);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    test_server1_.ServeFilesFromSourceDirectory("chrome/test/data");
    test_server2_.ServeFilesFromSourceDirectory("chrome/test/data");
    test_server1_.StartAcceptingConnections();
    test_server2_.StartAcceptingConnections();

    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  net::EmbeddedTestServer test_server1_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer test_server2_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::ScopedTestRoot scoped_test_root_;
};

// End-to-end test to verify that the --test-crs-constraints switch is honored
// when loading webpages in the browser. (More extensive testing of the various
// features of the switch is handled by unittests.)
IN_PROC_BROWSER_TEST_F(CertVerifierTestCrsConstraintsSwitchTest,
                       TestSwitchIsHonored) {
  // First server does not have any test constraints set, and should load
  // successfully.
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      test_server1_.GetURL("example.com", "/simple.html")));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      GetActiveWebContents()));

  // Second server has test constraints set for its root with a
  // max_version_exclusive of 0. The browser version should be greater than 0
  // so this root will not be trusted.
  EXPECT_FALSE(content::NavigateToURL(
      GetActiveWebContents(),
      test_server2_.GetURL("example.com", "/simple.html")));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      GetActiveWebContents()));
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

class CertVerifierUserSettingsTest : public PlatformBrowserTest {
 public:
  CertVerifierUserSettingsTest() {
    feature_list_.InitWithFeatures({features::kEnableCertManagementUIV2,
                                    features::kEnableCertManagementUIV2Write},
                                   {});
  }

  testing::AssertionResult AddCertificateToDatabaseAndWaitForVerifierUpdate(
      net::ServerCertificateDatabase::CertInformation cert_info) {
    base::test::TestFuture<void> cert_verifier_service_update_waiter;
    browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetCertVerifierServiceUpdater()
        ->WaitUntilNextUpdateForTesting(
            cert_verifier_service_update_waiter.GetCallback());
    base::test::TestFuture<bool> future;
    net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
        browser()->profile())
        ->AddOrUpdateUserCertificate(std::move(cert_info),
                                     future.GetCallback());
    if (!future.Get()) {
      return testing::AssertionFailure() << "database update failed";
    }
    if (!cert_verifier_service_update_waiter.Wait()) {
      return testing::AssertionFailure() << "wait for verifier update failed";
    }
    return testing::AssertionSuccess();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CertVerifierUserSettingsTest, TestUserSettingsUsed) {
  net::EmbeddedTestServer https_test_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer::ServerCertificateConfig test_cert_config;
  test_cert_config.intermediate =
      net::EmbeddedTestServer::IntermediateType::kMissing;
  test_cert_config.root = net::EmbeddedTestServer::RootType::kUniqueRoot;
  https_test_server.SetSSLConfig(test_cert_config);

  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  {
    scoped_refptr<net::X509Certificate> root_cert = https_test_server.GetRoot();
    net::ServerCertificateDatabase::CertInformation user_root_info;
    user_root_info.sha256hash_hex =
        base::HexEncode(crypto::SHA256Hash(root_cert->cert_span()));
    user_root_info.cert_metadata.mutable_trust()->set_trust_type(
        chrome_browser_server_certificate_database::CertificateTrust::
            CERTIFICATE_TRUST_TYPE_TRUSTED);
    user_root_info.der_cert = base::ToVector(root_cert->cert_span());

    ASSERT_TRUE(AddCertificateToDatabaseAndWaitForVerifierUpdate(
        std::move(user_root_info)));
  }
  {
    scoped_refptr<net::X509Certificate> hint_cert =
        https_test_server.GetGeneratedIntermediate();
    net::ServerCertificateDatabase::CertInformation user_hint_info;
    user_hint_info.sha256hash_hex =
        base::HexEncode(crypto::SHA256Hash(hint_cert->cert_span()));
    user_hint_info.cert_metadata.mutable_trust()->set_trust_type(
        chrome_browser_server_certificate_database::CertificateTrust::
            CERTIFICATE_TRUST_TYPE_UNSPECIFIED);
    user_hint_info.der_cert = base::ToVector(hint_cert->cert_span());

    ASSERT_TRUE(AddCertificateToDatabaseAndWaitForVerifierUpdate(
        std::move(user_hint_info)));
  }

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store + user settings.
  net::TestRootCerts::GetInstance()->Clear();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/simple.html")));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

IN_PROC_BROWSER_TEST_F(CertVerifierUserSettingsTest,
                       TestUserSettingsUsedAnchorConstraints) {
  net::EmbeddedTestServer https_test_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  // Use a certificate valid for the dns name localhost rather than an IP.
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);

  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  {
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    net::ServerCertificateDatabase::CertInformation user_root_info;
    user_root_info.sha256hash_hex =
        base::HexEncode(crypto::SHA256Hash(root_cert->cert_span()));
    user_root_info.cert_metadata.mutable_trust()->set_trust_type(
        chrome_browser_server_certificate_database::CertificateTrust::
            CERTIFICATE_TRUST_TYPE_TRUSTED);
    user_root_info.cert_metadata.mutable_constraints()->add_dns_names(
        "localhost");
    user_root_info.der_cert = base::ToVector(root_cert->cert_span());

    ASSERT_TRUE(AddCertificateToDatabaseAndWaitForVerifierUpdate(
        std::move(user_root_info)));
  }

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store + user settings.
  net::TestRootCerts::GetInstance()->Clear();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/simple.html")));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

IN_PROC_BROWSER_TEST_F(CertVerifierUserSettingsTest,
                       TestUserSettingsUsedAnchorConstraintsWrongConstraint) {
  net::EmbeddedTestServer https_test_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  // Use a certificate valid for the dns name localhost rather than an IP.
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);

  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  {
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    net::ServerCertificateDatabase::CertInformation user_root_info;
    user_root_info.sha256hash_hex =
        base::HexEncode(crypto::SHA256Hash(root_cert->cert_span()));
    user_root_info.cert_metadata.mutable_trust()->set_trust_type(
        chrome_browser_server_certificate_database::CertificateTrust::
            CERTIFICATE_TRUST_TYPE_TRUSTED);
    user_root_info.cert_metadata.mutable_constraints()->add_dns_names(
        "cruddyhost");
    user_root_info.der_cert = base::ToVector(root_cert->cert_span());

    ASSERT_TRUE(AddCertificateToDatabaseAndWaitForVerifierUpdate(
        std::move(user_root_info)));
  }

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store + user settings.
  net::TestRootCerts::GetInstance()->Clear();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/simple.html")));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

IN_PROC_BROWSER_TEST_F(CertVerifierUserSettingsTest,
                       TestUserSettingsUsedDistrusted) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);

  net::ServerCertificateDatabase::CertInformation cert_info;
  cert_info.sha256hash_hex =
      base::HexEncode(crypto::SHA256Hash(root_cert->cert_span()));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(
      chrome_browser_server_certificate_database::CertificateTrust::
          CERTIFICATE_TRUST_TYPE_DISTRUSTED);
  cert_info.der_cert = base::ToVector(root_cert->cert_span());

  ASSERT_TRUE(
      AddCertificateToDatabaseAndWaitForVerifierUpdate(std::move(cert_info)));

  // We don't clear test roots; the distrusted addition in the user db should
  // override the test root trust.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/simple.html")));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

IN_PROC_BROWSER_TEST_F(CertVerifierUserSettingsTest,
                       TestUserSettingsUsedDistrustedIncognito) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);

  net::ServerCertificateDatabase::CertInformation cert_info;
  cert_info.sha256hash_hex =
      base::HexEncode(crypto::SHA256Hash(root_cert->cert_span()));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(
      chrome_browser_server_certificate_database::CertificateTrust::
          CERTIFICATE_TRUST_TYPE_DISTRUSTED);
  cert_info.der_cert = base::ToVector(root_cert->cert_span());

  ASSERT_TRUE(
      AddCertificateToDatabaseAndWaitForVerifierUpdate(std::move(cert_info)));

  Browser* incognito_browser = CreateIncognitoBrowser();

  // We don't clear test roots; the distrusted addition in the user db should
  // override the test root trust, even for incognito.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser, https_test_server.GetURL("/simple.html")));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      incognito_browser->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(CertVerifierUserSettingsTest,
                       TestUserSettingsTrustedLeaf) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  scoped_refptr<net::X509Certificate> leaf_cert =
      https_test_server.GetCertificate();
  ASSERT_TRUE(leaf_cert);

  net::ServerCertificateDatabase::CertInformation cert_info;
  cert_info.sha256hash_hex =
      base::HexEncode(crypto::SHA256Hash(leaf_cert->cert_span()));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(
      chrome_browser_server_certificate_database::CertificateTrust::
          CERTIFICATE_TRUST_TYPE_TRUSTED);
  cert_info.der_cert = base::ToVector(leaf_cert->cert_span());

  // Sanity check.
  ASSERT_EQ(net::ServerCertificateDatabase::GetUserCertificateTrust(cert_info),
            bssl::CertificateTrustType::TRUSTED_LEAF);

  ASSERT_TRUE(
      AddCertificateToDatabaseAndWaitForVerifierUpdate(std::move(cert_info)));

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store + user settings.
  net::TestRootCerts::GetInstance()->Clear();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/simple.html")));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

IN_PROC_BROWSER_TEST_F(CertVerifierUserSettingsTest,
                       TestUserSettingsTrustedLeafAnchorAsLeaf) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig test_cert_config;
  test_cert_config.leaf_is_ca = true;
  test_cert_config.root = net::EmbeddedTestServer::RootType::kUniqueRoot;
  https_test_server.SetSSLConfig(test_cert_config);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  scoped_refptr<net::X509Certificate> leaf_cert =
      https_test_server.GetCertificate();
  ASSERT_TRUE(leaf_cert);

  net::ServerCertificateDatabase::CertInformation cert_info;
  cert_info.sha256hash_hex =
      base::HexEncode(crypto::SHA256Hash(leaf_cert->cert_span()));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(
      chrome_browser_server_certificate_database::CertificateTrust::
          CERTIFICATE_TRUST_TYPE_TRUSTED);
  cert_info.der_cert = base::ToVector(leaf_cert->cert_span());

  // Sanity check.
  ASSERT_EQ(net::ServerCertificateDatabase::GetUserCertificateTrust(cert_info),
            bssl::CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF);

  ASSERT_TRUE(
      AddCertificateToDatabaseAndWaitForVerifierUpdate(std::move(cert_info)));

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store + user settings.
  net::TestRootCerts::GetInstance()->Clear();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/simple.html")));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

IN_PROC_BROWSER_TEST_F(CertVerifierUserSettingsTest,
                       TestUserSettingsTrustedLeafAnchorAsAnchor) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig test_cert_config;
  test_cert_config.root_dns_names = {"example.com"};
  test_cert_config.root = net::EmbeddedTestServer::RootType::kUniqueRoot;
  https_test_server.SetSSLConfig(test_cert_config);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  scoped_refptr<net::X509Certificate> root_cert = https_test_server.GetRoot();
  ASSERT_TRUE(root_cert);

  net::ServerCertificateDatabase::CertInformation cert_info;
  cert_info.sha256hash_hex =
      base::HexEncode(crypto::SHA256Hash(root_cert->cert_span()));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(
      chrome_browser_server_certificate_database::CertificateTrust::
          CERTIFICATE_TRUST_TYPE_TRUSTED);
  cert_info.der_cert = base::ToVector(root_cert->cert_span());

  // Sanity check.
  ASSERT_EQ(net::ServerCertificateDatabase::GetUserCertificateTrust(cert_info),
            bssl::CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF);

  ASSERT_TRUE(
      AddCertificateToDatabaseAndWaitForVerifierUpdate(std::move(cert_info)));

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store + user settings.
  net::TestRootCerts::GetInstance()->Clear();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("example.com", "/simple.html")));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      chrome_test_utils::GetActiveWebContents(this)));
}

#if BUILDFLAG(IS_CHROMEOS)
class CertVerifierNSSMigrationTest : public PlatformBrowserTest {
 public:
  CertVerifierNSSMigrationTest() {
    const std::vector<base::test::FeatureRef> feature_flags = {
        features::kEnableCertManagementUIV2,
        features::kEnableCertManagementUIV2Write};
    if (GetTestPreCount() == 2) {
      feature_list_.InitWithFeatures(/*enabled_features=*/{},
                                     /*disabled_features=*/feature_flags);
    } else {
      feature_list_.InitWithFeatures(/*enabled_features=*/feature_flags,
                                     /*disabled_features=*/{});
    }
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Setup the NSS database before doing migration. The PRE_PRE_ test is run with
// the feature flag disabled so migration will not be attempted yet.
IN_PROC_BROWSER_TEST_F(CertVerifierNSSMigrationTest,
                       PRE_PRE_TestNSSCertMigration) {
  // PRE_ test and main test don't share state, so there isn't an easy way use a
  // generated EmbeddedTestServer cert in the PRE_ test and then run an
  // EmbeddedTestServer with the same generated cert in the main test. Therefore
  // we test the migration by importing the static test root and disabling
  // TestRootCerts.
  // Import test root as trusted in the NSS database.
  scoped_refptr<net::X509Certificate> test_root =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(test_root);
  base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
  NssServiceFactory::GetForContext(browser()->profile())
      ->UnsafelyGetNSSCertDatabaseForTesting(nss_waiter.GetCallback());
  net::NSSCertDatabase* nss_db = nss_waiter.Get();
  net::NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(nss_db->ImportCACerts(
      net::x509_util::CreateCERTCertificateListFromX509Certificate(
          test_root.get()),
      net::NSSCertDatabase::TRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  // Migration pref should be false.
  EXPECT_EQ(browser()->profile()->GetPrefs()->GetInteger(
                prefs::kNSSCertsMigratedToServerCertDb),
            static_cast<int>(net::ServerCertificateDatabaseService::
                                 NSSMigrationResultPref::kNotMigrated));
  histogram_tester_.ExpectTotalCount("Net.CertVerifier.NSSCertMigrationResult",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Net.CertVerifier.NSSCertMigrationQueuedRequestsWhenFinished", 0);
}

// Tests that when the feature flag is set, NSS cert migration is done on
// initialization and that the verification is blocked on the migration
// completing.
IN_PROC_BROWSER_TEST_F(CertVerifierNSSMigrationTest, PRE_TestNSSCertMigration) {
  net::EmbeddedTestServer https_test_server{
      net::EmbeddedTestServer::TYPE_HTTPS};

  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store.
  net::TestRootCerts::GetInstance()->Clear();
  // Loading the page should succeed since the root was trusted through the
  // server cert db.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/simple.html")));
  // TODO(https://crbug.com/40928765): This should not be
  // SECURE_WITH_POLICY_INSTALLED_CERT, it should just be SECURE. The
  // additional certs provided to the verifier need some additional flag to
  // indicate which ones are policy provided and which are user added.
  ssl_test_util::CheckSecurityState(
      chrome_test_utils::GetActiveWebContents(this),
      ssl_test_util::CertError::NONE,
      security_state::SECURE_WITH_POLICY_INSTALLED_CERT,
      ssl_test_util::AuthState::NONE);

  // Migration pref should be true now.
  EXPECT_EQ(
      browser()->profile()->GetPrefs()->GetInteger(
          prefs::kNSSCertsMigratedToServerCertDb),
      static_cast<int>(net::ServerCertificateDatabaseService::
                           NSSMigrationResultPref::kMigratedSuccessfully));

  // Migration histograms should have been recorded. ExpectUniqueSample is not
  // used here as the ChromeOS browsertests seem to create multiple users so
  // this histogram may be recorded multiple times (the other samples would be
  // kEmpty).
  histogram_tester_.ExpectBucketCount("Net.CertVerifier.NSSCertMigrationResult",
                                      net::ServerCertificateDatabaseService::
                                          NSSMigrationResultHistogram::kSuccess,
                                      1);
  // The Net.CertVerifier.NSSCertMigrationQueuedRequestsWhenFinished histogram
  // should have been recorded too, but it may not be possible to predict what
  // the buckets will be, so only verify that it was recorded at all.
  EXPECT_GT(histogram_tester_.GetTotalSum(
                "Net.CertVerifier.NSSCertMigrationQueuedRequestsWhenFinished"),
            0);

  // Set root cert in NSS to distrusted. This ensures that when the next phase
  // of the test runs it's actually the trust from the server cert db causing
  // the connection to succeed and not still using the NSS trust, and also
  // tests that the migration is not run again.
  scoped_refptr<net::X509Certificate> test_root =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(test_root);
  base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
  NssServiceFactory::GetForContext(browser()->profile())
      ->UnsafelyGetNSSCertDatabaseForTesting(nss_waiter.GetCallback());
  net::NSSCertDatabase* nss_db = nss_waiter.Get();
  nss_db->SetCertTrust(
      net::x509_util::CreateCERTCertificateFromX509Certificate(test_root.get())
          .get(),
      net::CertType::CA_CERT, net::NSSCertDatabase::DISTRUSTED_SSL);
}

// Tests that after migration is done the NSS user db is no longer depended on.
IN_PROC_BROWSER_TEST_F(CertVerifierNSSMigrationTest, TestNSSCertMigration) {
  // Migration pref should already be true.
  EXPECT_EQ(
      browser()->profile()->GetPrefs()->GetInteger(
          prefs::kNSSCertsMigratedToServerCertDb),
      static_cast<int>(net::ServerCertificateDatabaseService::
                           NSSMigrationResultPref::kMigratedSuccessfully));

  net::EmbeddedTestServer https_test_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store.
  net::TestRootCerts::GetInstance()->Clear();
  // Loading the page should succeed since the root was trusted through the
  // server cert db. The distrust set in NSS should be ignored as NSS user db
  // is no longer used.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/simple.html")));
  // TODO(https://crbug.com/40928765): This should not be
  // SECURE_WITH_POLICY_INSTALLED_CERT, it should just be SECURE. The
  // additional certs provided to the verifier need some additional flag to
  // indicate which ones are policy provided and which are user added.
  ssl_test_util::CheckSecurityState(
      chrome_test_utils::GetActiveWebContents(this),
      ssl_test_util::CertError::NONE,
      security_state::SECURE_WITH_POLICY_INSTALLED_CERT,
      ssl_test_util::AuthState::NONE);

  histogram_tester_.ExpectTotalCount("Net.CertVerifier.NSSCertMigrationResult",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Net.CertVerifier.NSSCertMigrationQueuedRequestsWhenFinished", 0);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
