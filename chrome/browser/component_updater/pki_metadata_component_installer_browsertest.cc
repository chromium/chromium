// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/certificate_transparency/certificate_transparency_config.pb.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#endif

namespace {

enum class CTEnforcement {
  // Enables CT enforcement.
  kEnabled,
  // Enables CT with Static CT API policy enforcement.
  kEnabledWithStaticCTEnforcement,
  // Disables CT enforcement via component updater proto.
  kDisabledByProto,
  // Disables CT enforcement via feature flag.
  kDisabledByFeature
};

int64_t SecondsSinceEpoch(base::Time t) {
  return (t - base::Time::UnixEpoch()).InSeconds();
}

// A CTLog generates a log identity private key, then computes and
// caches several properties from that key that are needed in test cases.
class CTLog {
 public:
  CTLog(std::string_view name,
        base::Time start,
        base::Time end,
        chrome_browser_certificate_transparency::CTLog::LogType type)
      : name_(name), start_(start), end_(end), type_(type) {}

  std::string_view name() const { return name_; }
  base::Time start() const { return start_; }
  base::Time end() const { return end_; }
  chrome_browser_certificate_transparency::CTLog::LogType type() const {
    return type_;
  }

  base::span<const uint8_t> spki() const { return spki_; }
  std::string_view spki_base64() const { return spki_base64_; }

  // Even though the id is just a span of bytes, so this should theoretically
  // return a base::span<const uint8_t> referencing the data we've cached, all the
  // call sites want it as a string.
  std::string id() const { return std::string(base::as_string_view(id_)); }
  std::string_view id_base64() const { return id_base64_; }

  bssl::UniquePtr<EVP_PKEY> key() { return bssl::UpRef(private_key_.key()); }

 private:
  const std::string name_;
  const base::Time start_;
  const base::Time end_;
  const chrome_browser_certificate_transparency::CTLog::LogType type_;

  // The generated private key and things derived from it. Note that the private
  // key itself can't be const, because returning a reference to it in key()
  // above requires mutating its inner refcount.
  crypto::keypair::PrivateKey private_key_{
      crypto::keypair::PrivateKey::GenerateEcP256()};
  const std::vector<uint8_t> spki_{private_key_.ToSubjectPublicKeyInfo()};
  const std::string spki_base64_{base::Base64Encode(spki_)};
  const std::array<uint8_t, crypto::hash::kSha256Size> id_{
      crypto::hash::Sha256(spki_)};
  const std::string id_base64_{base::Base64Encode(id_)};
};

void AddLogToCTConfig(chrome_browser_certificate_transparency::CTConfig* config,
                      const CTLog& log) {
  chrome_browser_certificate_transparency::CTLog* entry =
      config->mutable_log_list()->add_logs();
  entry->set_log_id(log.id_base64());
  entry->set_key(log.spki_base64());
  entry->set_purpose(chrome_browser_certificate_transparency::CTLog::PROD);
  entry->set_log_type(log.type());
  entry->mutable_temporal_interval()->mutable_start()->set_seconds(
      SecondsSinceEpoch(log.start()));
  entry->mutable_temporal_interval()->mutable_end()->set_seconds(
      SecondsSinceEpoch(log.end()));
  chrome_browser_certificate_transparency::CTLog_State* log_state =
      entry->add_state();
  log_state->set_current_state(
      chrome_browser_certificate_transparency::CTLog::USABLE);
  log_state->mutable_state_start()->set_seconds(SecondsSinceEpoch(log.start()));
  chrome_browser_certificate_transparency::CTLog_OperatorChange*
      operator_history = entry->add_operator_history();
  operator_history->set_name(log.name());
  operator_history->mutable_operator_start()->set_seconds(
      SecondsSinceEpoch(log.start()));
}

}  // namespace

namespace component_updater {

// TODO(crbug.com/341136041): add tests for pinning enforcement.
class PKIMetadataComponentUpdaterTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<CTEnforcement>,
      public PKIMetadataComponentInstallerService::Observer {
 public:
  PKIMetadataComponentUpdaterTest() {
    switch (GetParam()) {
      case CTEnforcement::kEnabled:
        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/
            {features::kCertificateTransparencyAskBeforeEnabling},
            /*disabled_features=*/{
                net::features::kEnableStaticCTAPIEnforcement});
        break;

      case CTEnforcement::kEnabledWithStaticCTEnforcement:
        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::
                                      kCertificateTransparencyAskBeforeEnabling,
                                  net::features::kEnableStaticCTAPIEnforcement},
            /*disabled_features=*/{});
        break;

      case CTEnforcement::kDisabledByProto:
        scoped_feature_list_.InitAndEnableFeature(
            features::kCertificateTransparencyAskBeforeEnabling);
        break;

      case CTEnforcement::kDisabledByFeature:
        scoped_feature_list_.InitAndDisableFeature(
            features::kCertificateTransparencyAskBeforeEnabling);
        break;
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    PKIMetadataComponentInstallerService::GetInstance()->AddObserver(this);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(component_dir_.CreateUniqueTempDir());
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up a configuration that will enable or disable CT enforcement
    // depending on the test parameter.
    chrome_browser_certificate_transparency::CTConfig ct_config;
    ct_config.set_disable_ct_enforcement(GetParam() ==
                                         CTEnforcement::kDisabledByProto);
    ct_config.mutable_log_list()->mutable_timestamp()->set_seconds(
        SecondsSinceEpoch(base::Time::Now()));
    ASSERT_TRUE(PKIMetadataComponentInstallerService::GetInstance()
                    ->WriteCTDataForTesting(component_dir_.GetPath(),
                                            ct_config.SerializeAsString()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    PKIMetadataComponentInstallerService::GetInstance()->RemoveObserver(this);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Wait for configuration set in `SetUpInProcessBrowserTestFixture` to load.
    WaitForPKIConfiguration(1);
  }

 protected:
  // Waits for the PKI to have been configured at least |expected_times|.
  void WaitForPKIConfiguration(int expected_times) {
    if (GetParam() == CTEnforcement::kDisabledByFeature) {
      // When CT is disabled by the feature flag there are no callbacks to
      // wait on, so just spin the runloop.
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(pki_metadata_configured_times_, 0);
    } else {
      expected_pki_metadata_configured_times_ = expected_times;
      if (pki_metadata_configured_times_ >=
          expected_pki_metadata_configured_times_) {
        return;
      }
      base::RunLoop run_loop;
      pki_metadata_config_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  const base::FilePath& GetComponentDirPath() const {
    return component_dir_.GetPath();
  }

  bool is_ct_enforced() const {
    return GetParam() == CTEnforcement::kEnabled ||
           GetParam() == CTEnforcement::kEnabledWithStaticCTEnforcement;
  }

  void DoTestAtLeastOneRFC6962LogPolicy(
      chrome_browser_certificate_transparency::CTLog::LogType log_type,
      bool expect_ct_error);

 private:
  void OnCTLogListConfigured() override {
    ++pki_metadata_configured_times_;
    if (pki_metadata_config_closure_ &&
        pki_metadata_configured_times_ >=
            expected_pki_metadata_configured_times_) {
      std::move(pki_metadata_config_closure_).Run();
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir component_dir_;

  base::OnceClosure pki_metadata_config_closure_;
  int expected_pki_metadata_configured_times_ = 0;
  int pki_metadata_configured_times_ = 0;
};

// Tests that the PKI Metadata configuration is recovered after a network
// service restart.
IN_PROC_BROWSER_TEST_P(PKIMetadataComponentUpdaterTest,
                       ReloadsPKIMetadataConfigAfterCrash) {
  // Network service is not running out of process, so cannot be crashed.
  if (!content::IsOutOfProcessNetworkService()) {
    return;
  }

  // Make the test root be interpreted as a known root so that CT will be
  // required.
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  net::ScopedTestKnownRoot scoped_known_root(root_cert.get());

  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  static constexpr char kHostname[] = "example.com";
  https_server_ok.SetCertHostnames({kHostname});
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL(kHostname, "/simple.html")));

  // Check that the page is blocked depending on CT enforcement.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  if (is_ct_enforced()) {
    EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  } else {
    EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  }

  // Restart the network service.
  SimulateNetworkServiceCrash();
  // Wait for the restarted network service to load the component update data
  // that is already on disk.
  WaitForPKIConfiguration(2);

  // Check that the page is still blocked depending on CT enforcement.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL(kHostname, "/simple.html")));
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  if (is_ct_enforced()) {
    EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  } else {
    EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  }
}

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentUpdaterTest, TestCTUpdate) {
  const base::Time kLogStart = base::Time::Now() - base::Days(1);
  const base::Time kLogEnd = base::Time::Now() + base::Days(1);

  CTLog log1("log operator 1", kLogStart, kLogEnd,
             chrome_browser_certificate_transparency::CTLog::RFC6962);
  CTLog log2(
      "log operator 2", kLogStart, kLogEnd,
      chrome_browser_certificate_transparency::CTLog::LOG_TYPE_UNSPECIFIED);

  // Make the test root be interpreted as a known root so that CT will be
  // required.
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  net::ScopedTestKnownRoot scoped_known_root(root_cert.get());

  // Start a test server that uses a certificate with SCTs for the above test
  // logs.
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig server_config;
  // The same hostname is used for each request, which verifies that the CT log
  // updates cause verifier caches and socket pool invalidation, so that the
  // next request for the same host will use the updated CT state.
  server_config.dns_names = {"example.com"};
  server_config.embedded_scts.emplace_back(log1.id(), log1.key(),
                                           base::Time::Now());
  server_config.embedded_scts.emplace_back(log2.id(), log2.key(),
                                           base::Time::Now());
  https_server_ok.SetSSLConfig(server_config);

  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  // Check that the page is blocked depending on CT enforcement.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("example.com", "/simple.html")));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  if (is_ct_enforced()) {
    EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  } else {
    EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  }

  // Update with a CT configuration that trusts log1 and log2
  //
  // Set up a configuration that will enable or disable CT enforcement
  // depending on the test parameter.
  chrome_browser_certificate_transparency::CTConfig ct_config;
  ct_config.set_disable_ct_enforcement(GetParam() ==
                                       CTEnforcement::kDisabledByProto);
  ct_config.mutable_log_list()->mutable_timestamp()->set_seconds(
      SecondsSinceEpoch(base::Time::Now()));
  AddLogToCTConfig(&ct_config, log1);
  AddLogToCTConfig(&ct_config, log2);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(PKIMetadataComponentInstallerService::GetInstance()
                    ->WriteCTDataForTesting(GetComponentDirPath(),
                                            ct_config.SerializeAsString()));
  }

  // Should be trusted now.
  PKIMetadataComponentInstallerService::GetInstance()
      ->ReconfigureAfterNetworkRestart();
  WaitForPKIConfiguration(2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("example.com", "/simple.html")));
  EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());

  // Update CT configuration again with the same CT logs but mark the 1st log
  // as retired.
  {
    chrome_browser_certificate_transparency::CTLog* log =
        ct_config.mutable_log_list()->mutable_logs(0);
    log->clear_state();
    // Log states are in reverse chronological order, so the most recent state
    // comes first.
    {
      chrome_browser_certificate_transparency::CTLog_State* log_state =
          log->add_state();
      log_state->set_current_state(
          chrome_browser_certificate_transparency::CTLog::RETIRED);
      log_state->mutable_state_start()->set_seconds(
          SecondsSinceEpoch(kLogStart) + 1);
    }
    {
      chrome_browser_certificate_transparency::CTLog_State* log_state =
          log->add_state();
      log_state->set_current_state(
          chrome_browser_certificate_transparency::CTLog::USABLE);
      log_state->mutable_state_start()->set_seconds(
          SecondsSinceEpoch(kLogStart));
    }
  }
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(PKIMetadataComponentInstallerService::GetInstance()
                    ->WriteCTDataForTesting(GetComponentDirPath(),
                                            ct_config.SerializeAsString()));
  }

  // Should be untrusted again since 2 logs are required for diversity. Both
  // SCTs should verify successfully but only one of them is accepted as the
  // other has a timestamp after the log retirement state change timestamp.
  PKIMetadataComponentInstallerService::GetInstance()
      ->ReconfigureAfterNetworkRestart();
  WaitForPKIConfiguration(3);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("example.com", "/simple.html")));
  if (is_ct_enforced()) {
    EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  } else {
    EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  }
}

// Tests that at least one RFC6962 log policy is correctly applied when Static
// CT API enforcement is enabled. All logs in the test will be set to
// `log_type`. If `expect_ct_error_with_static_ct_api_enforcement` is true,
// CT checks with Static CT API enforcement should cause an SSL error.
void PKIMetadataComponentUpdaterTest::DoTestAtLeastOneRFC6962LogPolicy(
    chrome_browser_certificate_transparency::CTLog::LogType log_type,
    bool expect_ct_error_with_static_ct_api_enforcement) {
  const base::Time kLogStart = base::Time::Now() - base::Days(1);
  const base::Time kLogEnd = base::Time::Now() + base::Days(1);
  CTLog log1("log operator 1", kLogStart, kLogEnd, log_type);
  CTLog log2("log operator 2", kLogStart, kLogEnd, log_type);

  // Make the test root be interpreted as a known root so that CT will be
  // required.
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  net::ScopedTestKnownRoot scoped_known_root(root_cert.get());

  // Start a test server that uses a certificate with SCTs for the above test
  // logs.
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig server_config;
  // The same hostname is used for each request, which verifies that the CT log
  // updates cause verifier caches and socket pool invalidation, so that the
  // next request for the same host will use the updated CT state.
  server_config.dns_names = {"example.com"};
  server_config.embedded_scts.emplace_back(log1.id(), log1.key(),
                                           base::Time::Now());
  server_config.embedded_scts.emplace_back(log2.id(), log2.key(),
                                           base::Time::Now());
  https_server_ok.SetSSLConfig(server_config);

  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  // Check that the page is blocked depending on CT enforcement.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("example.com", "/simple.html")));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  if (is_ct_enforced()) {
    EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  } else {
    EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  }

  // Update with a CT configuration that trusts log1 and log2. Neither of
  // these logs is RFC6962, so the SCTs will not pass validation.
  //
  // Set up a configuration that will enable or disable CT enforcement
  // depending on the test parameter.
  chrome_browser_certificate_transparency::CTConfig ct_config;
  ct_config.set_disable_ct_enforcement(GetParam() ==
                                       CTEnforcement::kDisabledByProto);
  ct_config.mutable_log_list()->mutable_timestamp()->set_seconds(
      SecondsSinceEpoch(base::Time::Now()));
  AddLogToCTConfig(&ct_config, log1);
  AddLogToCTConfig(&ct_config, log2);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(PKIMetadataComponentInstallerService::GetInstance()
                    ->WriteCTDataForTesting(GetComponentDirPath(),
                                            ct_config.SerializeAsString()));
  }

  PKIMetadataComponentInstallerService::GetInstance()
      ->ReconfigureAfterNetworkRestart();
  WaitForPKIConfiguration(2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("example.com", "/simple.html")));

  if (GetParam() == CTEnforcement::kEnabledWithStaticCTEnforcement) {
    if (expect_ct_error_with_static_ct_api_enforcement) {
      EXPECT_NE(u"OK",
                chrome_test_utils::GetActiveWebContents(this)->GetTitle());
    } else {
      EXPECT_EQ(u"OK",
                chrome_test_utils::GetActiveWebContents(this)->GetTitle());
    }
  } else {
    EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  }
}

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentUpdaterTest,
                       TestAtLeastOneRFC6962LogPolicy_StaticCTAPILogs) {
  // Test with all logs with Static CT API type. Since at least one RFC6962 log
  // is expected, this should show an SSL error caused by CT.
  DoTestAtLeastOneRFC6962LogPolicy(
      chrome_browser_certificate_transparency::CTLog::STATIC_CT_API,
      /*expect_ct_error_with_static_ct_api_enforcement=*/true);
}

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentUpdaterTest,
                       TestAtLeastOneRFC6962LogPolicy_UnspecifiedLogTypes) {
  // Test with all logs with unspecified type. These are treated as RFC6962
  // logs so they shouldn't cause an SSL error.
  // TODO(crbug.com/370724580): Disallow unspecified log type once all logs in
  // the hardcoded and component updater protos have proper log types.
  DoTestAtLeastOneRFC6962LogPolicy(
      chrome_browser_certificate_transparency::CTLog::LOG_TYPE_UNSPECIFIED,
      /*expect_ct_error_with_static_ct_api_enforcement=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    PKIMetadataComponentUpdater,
    PKIMetadataComponentUpdaterTest,
    testing::Values(CTEnforcement::kEnabled,
                    CTEnforcement::kEnabledWithStaticCTEnforcement,
                    CTEnforcement::kDisabledByProto,
                    CTEnforcement::kDisabledByFeature));

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

class PKIMetadataComponentChromeRootStoreUpdateTest
    : public InProcessBrowserTest,
      public PKIMetadataComponentInstallerService::Observer {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        false);
    PKIMetadataComponentInstallerService::GetInstance()->AddObserver(this);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(component_dir_.CreateUniqueTempDir());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownInProcessBrowserTestFixture() override {
    PKIMetadataComponentInstallerService::GetInstance()->RemoveObserver(this);
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
  }

  class CRSWaiter {
   public:
    explicit CRSWaiter(PKIMetadataComponentChromeRootStoreUpdateTest* test) {
      test_ = test;
      test_->crs_config_closure_ = run_loop_.QuitClosure();
    }
    void Wait() { run_loop_.Run(); }

   private:
    base::RunLoop run_loop_;
    raw_ptr<PKIMetadataComponentChromeRootStoreUpdateTest> test_;
  };

  void InstallCRSUpdate(chrome_root_store::RootStore root_store_proto) {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(
          PKIMetadataComponentInstallerService::GetInstance()
              ->WriteCRSDataForTesting(component_dir_.GetPath(),
                                       root_store_proto.SerializeAsString()));
    }

    CRSWaiter waiter(this);
    PKIMetadataComponentInstallerService::GetInstance()
        ->ConfigureChromeRootStore();
    waiter.Wait();
  }

  void InstallCRSUpdate(const std::vector<std::string>& der_roots) {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++last_used_crs_version_);
    for (const auto& der_root : der_roots) {
      root_store_proto.add_trust_anchors()->set_der(der_root);
    }

    InstallCRSUpdate(std::move(root_store_proto));
  }

 protected:
  base::ScopedTempDir component_dir_;

 private:
  void OnChromeRootStoreConfigured() override {
    if (crs_config_closure_) {
      std::move(crs_config_closure_).Run();
    }
  }

  base::OnceClosure crs_config_closure_;
  int64_t last_used_crs_version_ = net::CompiledChromeRootStoreVersion();
};

IN_PROC_BROWSER_TEST_F(PKIMetadataComponentChromeRootStoreUpdateTest,
                       CheckCRSUpdate) {
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig server_config;
  server_config.dns_names = {"*.example.com"};
  https_server_ok.SetSSLConfig(server_config);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");

  // Clear test roots so that cert validation only happens with
  // what's in Chrome Root Store.
  net::TestRootCerts::GetInstance()->Clear();

  ASSERT_TRUE(https_server_ok.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("a.example.com", "/simple.html")));

  // Check that the page is blocked depending on contents of Chrome Root Store.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_NE(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_AUTHORITY_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);

  {
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    InstallCRSUpdate({std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()))});
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("b.example.com", "/simple.html")));

  // Check that the page is allowed due to contents of Chrome Root Store.
  tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_EQ(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticatedState(tab, ssl_test_util::AuthState::NONE);

  {
    // We reject empty CRS updates, so create a new cert root that doesn't match
    // what the test server uses.
    auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
    InstallCRSUpdate({root->GetDER()});
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("c.example.com", "/simple.html")));

  // Check that the page is blocked depending on contents of Chrome Root Store.
  tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_NE(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_AUTHORITY_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);
}

// Similar to CheckCRSUpdate, except using the same hostname for all requests.
// This tests whether the CRS update causes cached verification results to be
// disregarded.
IN_PROC_BROWSER_TEST_F(PKIMetadataComponentChromeRootStoreUpdateTest,
                       CheckCRSUpdateAffectsCachedVerifications) {
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig server_config;
  server_config.dns_names = {"*.example.com"};
  https_server_ok.SetSSLConfig(server_config);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");

  // Clear test roots so that cert validation only happens with
  // what's in Chrome Root Store.
  net::TestRootCerts::GetInstance()->Clear();

  static constexpr char kHostname[] = "a.example.com";

  ASSERT_TRUE(https_server_ok.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL(kHostname, "/simple.html")));

  // Check that the page is blocked depending on contents of Chrome Root Store.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_NE(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_AUTHORITY_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);

  {
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    InstallCRSUpdate({std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()))});
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL(kHostname, "/title2.html")));

  // Check that the page is allowed due to contents of Chrome Root Store.
  tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_EQ(chrome_test_utils::GetActiveWebContents(this)->GetTitle(),
            u"Title Of Awesomeness");
  ssl_test_util::CheckAuthenticatedState(tab, ssl_test_util::AuthState::NONE);

  {
    // We reject empty CRS updates, so create a new cert root that doesn't match
    // what the test server uses.
    auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
    InstallCRSUpdate({root->GetDER()});
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL(kHostname, "/title3.html")));

  // Check that the page is blocked depending on contents of Chrome Root Store.
  tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_NE(chrome_test_utils::GetActiveWebContents(this)->GetTitle(),
            u"Title Of Awesomeness");
  EXPECT_NE(chrome_test_utils::GetActiveWebContents(this)->GetTitle(),
            u"Title Of More Awesomeness");
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_AUTHORITY_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);
}

IN_PROC_BROWSER_TEST_F(PKIMetadataComponentChromeRootStoreUpdateTest,
                       CheckCRSUpdateDnsConstraint) {
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig server_config;
  server_config.dns_names = {"*.example.com"};
  https_server_ok.SetSSLConfig(server_config);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");

  // Clear test roots so that cert validation only happens with
  // what's in Chrome Root Store.
  net::TestRootCerts::GetInstance()->Clear();

  ASSERT_TRUE(https_server_ok.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("a.example.com", "/simple.html")));

  // The page should be blocked as the test root is not trusted yet.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_NE(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_AUTHORITY_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);

  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  // Install CRS update that trusts root with a constraint that matches the
  // leaf's subjectAltName.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->add_constraints()->add_permitted_dns_names("example.com");

    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("b.example.com", "/simple.html")));

  // Check that the page is allowed now.
  tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_EQ(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticatedState(tab, ssl_test_util::AuthState::NONE);

  // Install CRS update that trusts root with a constraint that does not match
  // the leaf's subjectAltName.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->add_constraints()->add_permitted_dns_names("example.org");

    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("c.example.com", "/simple.html")));

  // Check that the page is blocked now.
  tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_NE(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_AUTHORITY_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);
}

class PKIMetadataComponentChromeRootStoreUpdateQwacTest
    : public PKIMetadataComponentChromeRootStoreUpdateTest,
      public testing::WithParamInterface<bool> {
 public:
  PKIMetadataComponentChromeRootStoreUpdateQwacTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(net::features::kVerifyQWACs);
    } else {
      feature_list_.InitAndDisableFeature(net::features::kVerifyQWACs);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PKIMetadataComponentChromeRootStoreUpdateQwacTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentChromeRootStoreUpdateQwacTest,
                       CheckCrsEutlUpdate) {
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig server_config;
  server_config.dns_names = {"*.example.com"};
  // Set policy OIDs and QWAC QC types on the leaf so that it will validate as
  // a QWAC. Also include an intermediate so we can set the intermediate as
  // part of the EUTL trust store in the CRS update.
  // OIDs: CABF OV, ETSI QNCP-w
  server_config.policy_oids = {"2.23.140.1.2.2", "0.4.0.194112.1.5"};
  server_config.qwac_qc_types = {bssl::der::Input(net::kEtsiQctWebOid)};
  server_config.intermediate =
      net::EmbeddedTestServer::IntermediateType::kInHandshake;
  https_server_ok.SetSSLConfig(server_config);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");

  // Install only the root cert as a trust anchor in CRS and check that the
  // page load is successful but the cert is not a valid QWAC.
  net::TestRootCerts::GetInstance()->Clear();
  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  {
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    auto* trust_anchor = root_store_proto.add_trust_anchors();
    trust_anchor->set_der(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()));
    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(https_server_ok.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("a.example.com", "/simple.html")));

  // Check that the page's cert status is not a QWAC.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_EQ(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticatedState(tab, ssl_test_util::AuthState::NONE);
  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  net::CertStatus cert_status = entry->GetSSL().cert_status;
  EXPECT_FALSE(cert_status & net::CERT_STATUS_IS_QWAC);

  // Install CRS update that has the root as a trust anchor in CRS and the
  // intermediate as a QWAC issuer.
  {
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    scoped_refptr<net::X509Certificate> intermediate_cert =
        https_server_ok.GetGeneratedIntermediate();
    ASSERT_TRUE(intermediate_cert);

    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    auto* trust_anchor = root_store_proto.add_trust_anchors();
    trust_anchor->set_der(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()));
    auto* additional_cert = root_store_proto.add_additional_certs();
    additional_cert->set_der(net::x509_util::CryptoBufferAsStringPiece(
        intermediate_cert->cert_buffer()));
    additional_cert->set_eutl(true);
    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("b.example.com", "/simple.html")));

  // Check the page's cert status is a QWAC (if net::features::kVerifyQWACs is
  // enabled).
  tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_EQ(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticatedState(tab, ssl_test_util::AuthState::NONE);
  cert_status = tab->GetController().GetVisibleEntry()->GetSSL().cert_status;
  EXPECT_EQ(GetParam(), !!(cert_status & net::CERT_STATUS_IS_QWAC));

  // Install a CRS update that has the root as both a trust anchor in CRS and
  // a QWAC issuer
  {
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    auto* trust_anchor = root_store_proto.add_trust_anchors();
    trust_anchor->set_der(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()));
    trust_anchor->set_eutl(true);
    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("c.example.com", "/simple.html")));

  // Check the page's cert status is a QWAC (if net::features::kVerifyQWACs is
  // enabled).
  tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  EXPECT_EQ(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
  ssl_test_util::CheckAuthenticatedState(tab, ssl_test_util::AuthState::NONE);
  cert_status = tab->GetController().GetVisibleEntry()->GetSSL().cert_status;
  EXPECT_EQ(GetParam(), !!(cert_status & net::CERT_STATUS_IS_QWAC));
}

// Test suite for tests that depend on both Certificate Transparency and Chrome
// Root Store updates.
class PKIMetadataComponentCtAndCrsUpdaterTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<CTEnforcement>,
      public PKIMetadataComponentInstallerService::Observer {
 public:
  PKIMetadataComponentCtAndCrsUpdaterTest() {
    if (GetParam() == CTEnforcement::kDisabledByFeature) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
              net::features::kChromeRootStoreUsed
#endif
          },
          /*disabled_features=*/{
              features::kCertificateTransparencyAskBeforeEnabling});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kCertificateTransparencyAskBeforeEnabling,
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
           net::features::kChromeRootStoreUsed
#endif
          },
          /*disabled_features=*/{});
    }
  }
  void SetUpInProcessBrowserTestFixture() override {
    PKIMetadataComponentInstallerService::GetInstance()->AddObserver(this);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(component_dir_.CreateUniqueTempDir());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownInProcessBrowserTestFixture() override {
    PKIMetadataComponentInstallerService::GetInstance()->RemoveObserver(this);
  }

 protected:
  // Waits for the CT log lists to have been configured at least
  // |expected_times|.
  void WaitForCtConfiguration(int expected_times) {
    if (GetParam() == CTEnforcement::kDisabledByFeature) {
      // When CT is disabled by the feature flag there are no callbacks to
      // wait on, so just spin the runloop.
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(ct_log_list_configured_times_, 0);
    } else {
      expected_ct_log_list_configured_times_ = expected_times;
      if (ct_log_list_configured_times_ >=
          expected_ct_log_list_configured_times_) {
        return;
      }
      base::RunLoop run_loop;
      pki_metadata_config_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  const base::FilePath& GetComponentDirPath() const {
    return component_dir_.GetPath();
  }

  void InstallCRSUpdate(chrome_root_store::RootStore root_store_proto) {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(
          PKIMetadataComponentInstallerService::GetInstance()
              ->WriteCRSDataForTesting(component_dir_.GetPath(),
                                       root_store_proto.SerializeAsString()));
    }

    CRSWaiter waiter(this);
    PKIMetadataComponentInstallerService::GetInstance()
        ->ConfigureChromeRootStore();
    waiter.Wait();
  }

 private:
  void OnCTLogListConfigured() override {
    ++ct_log_list_configured_times_;
    if (pki_metadata_config_closure_ &&
        ct_log_list_configured_times_ >=
            expected_ct_log_list_configured_times_) {
      std::move(pki_metadata_config_closure_).Run();
    }
  }

  void OnChromeRootStoreConfigured() override {
    if (crs_config_closure_) {
      std::move(crs_config_closure_).Run();
    }
  }

  class CRSWaiter {
   public:
    explicit CRSWaiter(PKIMetadataComponentCtAndCrsUpdaterTest* test) {
      test_ = test;
      test_->crs_config_closure_ = run_loop_.QuitClosure();
    }
    void Wait() { run_loop_.Run(); }

   private:
    base::RunLoop run_loop_;
    raw_ptr<PKIMetadataComponentCtAndCrsUpdaterTest> test_;
  };

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir component_dir_;

  base::OnceClosure pki_metadata_config_closure_;
  int expected_ct_log_list_configured_times_ = 0;
  int ct_log_list_configured_times_ = 0;
  base::OnceClosure crs_config_closure_;
  int64_t last_used_crs_version_ = net::CompiledChromeRootStoreVersion();
};

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentCtAndCrsUpdaterTest,
                       TestChromeRootStoreConstraintsSct) {
  const base::Time kLogStart = base::Time::Now() - base::Days(1);
  const base::Time kLogEnd = base::Time::Now() + base::Days(1);
  CTLog log1("log operator 1", kLogStart, kLogEnd,
             chrome_browser_certificate_transparency::CTLog::RFC6962);
  CTLog log2(
      "log operator 2", kLogStart, kLogEnd,
      chrome_browser_certificate_transparency::CTLog::LOG_TYPE_UNSPECIFIED);
  CTLog unknown_log(
      "unknown log operator", kLogStart, kLogEnd,
      chrome_browser_certificate_transparency::CTLog::LOG_TYPE_UNSPECIFIED);

  const base::Time kSctTime0UnknownLog = base::Time::Now() - base::Minutes(30);
  const base::Time kSctTime1 = base::Time::Now() - base::Minutes(20);
  const base::Time kSctTime2 = base::Time::Now() - base::Minutes(10);

  // Start a test server that uses a certificate with SCTs for the above test
  // logs.
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig server_config;
  server_config.dns_names = {"*.example.com"};
  server_config.embedded_scts.emplace_back(log1.id(), log1.key(), kSctTime1);
  server_config.embedded_scts.emplace_back(log2.id(), log2.key(), kSctTime2);
  server_config.embedded_scts.emplace_back(unknown_log.id(), unknown_log.key(),
                                           kSctTime0UnknownLog);
  https_server_ok.SetSSLConfig(server_config);

  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  // Clear test roots so that cert validation only happens with
  // what's in Chrome Root Store.
  net::TestRootCerts::GetInstance()->Clear();

  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  int64_t crs_version = net::CompiledChromeRootStoreVersion();

  // Install CRS update that trusts root without constraints.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));

    InstallCRSUpdate(std::move(root_store_proto));
  }

  // Install CT configuration that trusts log1 and log2.
  //
  // Set up a configuration that will enable or disable CT enforcement
  // depending on the test parameter.
  chrome_browser_certificate_transparency::CTConfig ct_config;
  ct_config.set_disable_ct_enforcement(GetParam() ==
                                       CTEnforcement::kDisabledByProto);
  ct_config.mutable_log_list()->mutable_timestamp()->set_seconds(
      SecondsSinceEpoch(base::Time::Now()));
  AddLogToCTConfig(&ct_config, log1);
  AddLogToCTConfig(&ct_config, log2);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(PKIMetadataComponentInstallerService::GetInstance()
                    ->WriteCTDataForTesting(GetComponentDirPath(),
                                            ct_config.SerializeAsString()));
  }

  PKIMetadataComponentInstallerService::GetInstance()
      ->ReconfigureAfterNetworkRestart();
  WaitForCtConfiguration(1);

  // Should be trusted.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("b.example.com", "/simple.html")));
  EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());

  // Install CRS update that trusts root with a SCTNotAfter constraint.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->add_constraints()->set_sct_not_after_sec(
        SecondsSinceEpoch(kSctTime1 + base::Seconds(1)));

    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("c.example.com", "/simple.html")));
  // Should be trusted if CT is enabled since the SCTNotAfter constraint is
  // satisfied by the SCT from log1. Should be trusted if CT feature is
  // disabled since SCTNotAfter fails open when CT is disabled.
  EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());

  // Install CRS update that trusts root with a SCTNotAfter constraint that is
  // before both of the valid SCTs.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->add_constraints()->set_sct_not_after_sec(
        SecondsSinceEpoch(kSctTime0UnknownLog + base::Seconds(1)));

    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("c.example.com", "/simple.html")));
  switch (GetParam()) {
    case CTEnforcement::kEnabled:
    case CTEnforcement::kEnabledWithStaticCTEnforcement:
      // Should be distrusted if CT is enabled. The SCTNotAfter constraint is
      // not satisfied by any valid SCT. The SCT from the unknown log is not
      // counted even though the timestamp matches the constraint.
      EXPECT_NE(u"OK",
                chrome_test_utils::GetActiveWebContents(this)->GetTitle());
      break;
    case CTEnforcement::kDisabledByProto:
    case CTEnforcement::kDisabledByFeature:
      // Should be trusted if CT feature is disabled since SCTNotAfter fails
      // open when CT is disabled.
      EXPECT_EQ(u"OK",
                chrome_test_utils::GetActiveWebContents(this)->GetTitle());
      break;
  }

  // Install CRS update that trusts root with a SCTAllAfter constraint that is
  // before both of the valid SCTs.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->add_constraints()->set_sct_all_after_sec(
        SecondsSinceEpoch(kSctTime1 - base::Seconds(1)));

    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("c.example.com", "/simple.html")));
  // Should be trusted if CT is enabled since the SCTAlltAfter constraint is
  // satisfied by the SCT from both logs.
  // Should be trusted if CT feature is disabled since SCTAllAfter fails
  // open when CT is disabled.
  EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());

  // Install CRS update that trusts root with a SCTAllAfter constraint that is
  // before one of the SCTs but after the other.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->add_constraints()->set_sct_all_after_sec(
        SecondsSinceEpoch(kSctTime1 + base::Seconds(1)));

    InstallCRSUpdate(std::move(root_store_proto));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("c.example.com", "/simple.html")));
  switch (GetParam()) {
    case CTEnforcement::kEnabled:
    case CTEnforcement::kEnabledWithStaticCTEnforcement:
      // Should be distrusted since one of the SCTs was before the SCTAllAfter
      // constraint.
      EXPECT_NE(u"OK",
                chrome_test_utils::GetActiveWebContents(this)->GetTitle());
      break;
    case CTEnforcement::kDisabledByProto:
    case CTEnforcement::kDisabledByFeature:
      // Should be trusted if CT feature is disabled since SCTAllAfter fails
      // open when CT is disabled.
      EXPECT_EQ(u"OK",
                chrome_test_utils::GetActiveWebContents(this)->GetTitle());
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    PKIMetadataComponentUpdater,
    PKIMetadataComponentCtAndCrsUpdaterTest,
    testing::Values(CTEnforcement::kEnabled,
                    CTEnforcement::kEnabledWithStaticCTEnforcement,
                    CTEnforcement::kDisabledByProto,
                    CTEnforcement::kDisabledByFeature));

// TODO(crbug.com/40816087) additional Chrome Root Store browser tests to
// add:
//
// * Test that AIA fetching still works after updating CRS.
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

}  // namespace component_updater
