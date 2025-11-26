// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/certificate_transparency/certificate_transparency_config.pb.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/util.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_doh_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "base/test/bind.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#endif

namespace {

enum class CTEnforcement {
  // Enables CT enforcement.
  kEnabled,
  // Enables CT with one-6962-log CT policy enforcement.
  kEnabledWithOne6962Enforcement,
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
            /*disabled_features=*/{net::features::kEnforceOneRfc6962CtPolicy});
        break;

      case CTEnforcement::kEnabledWithOne6962Enforcement:
        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::
                                      kCertificateTransparencyAskBeforeEnabling,
                                  net::features::kEnforceOneRfc6962CtPolicy},
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
           GetParam() == CTEnforcement::kEnabledWithOne6962Enforcement;
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
// `log_type`. If `expect_ct_error_with_one_6962_policy_enforcement` is true,
// CT checks with Static CT API enforcement should cause an SSL error.
void PKIMetadataComponentUpdaterTest::DoTestAtLeastOneRFC6962LogPolicy(
    chrome_browser_certificate_transparency::CTLog::LogType log_type,
    bool expect_ct_error_with_one_6962_policy_enforcement) {
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

  if (is_ct_enforced()) {
    if (expect_ct_error_with_one_6962_policy_enforcement) {
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
  // is expected when the one-6962 policy is enabled, this should show an error.
  DoTestAtLeastOneRFC6962LogPolicy(
      chrome_browser_certificate_transparency::CTLog::STATIC_CT_API,
      /*expect_ct_error_with_one_6962_policy_enforcement=*/true);
}

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentUpdaterTest,
                       TestAtLeastOneRFC6962LogPolicy_UnspecifiedLogTypes) {
  // Test with all logs with unspecified type. These are treated as RFC6962
  // logs so they shouldn't cause an SSL error.
  // TODO(crbug.com/370724580): Disallow unspecified log type once all logs in
  // the hardcoded and component updater protos have proper log types.
  DoTestAtLeastOneRFC6962LogPolicy(
      chrome_browser_certificate_transparency::CTLog::LOG_TYPE_UNSPECIFIED,
      /*expect_ct_error_with_one_6962_policy_enforcement=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    PKIMetadataComponentUpdater,
    PKIMetadataComponentUpdaterTest,
    testing::Values(CTEnforcement::kEnabled,
                    CTEnforcement::kEnabledWithOne6962Enforcement,
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

  class MtcMetadataWaiter {
   public:
    explicit MtcMetadataWaiter(
        PKIMetadataComponentChromeRootStoreUpdateTest* test) {
      test_ = test;
      test_->mtc_metadata_config_closure_ = run_loop_.QuitClosure();
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

  void InstallMtcMetadataUpdate(
      chrome_root_store::MtcMetadata mtc_metadata_proto) {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(PKIMetadataComponentInstallerService::GetInstance()
                      ->WriteMtcMetadataForTesting(
                          component_dir_.GetPath(),
                          mtc_metadata_proto.SerializeAsString()));
    }

    MtcMetadataWaiter waiter(this);
    PKIMetadataComponentInstallerService::GetInstance()->ConfigureMtcMetadata();
    waiter.Wait();
  }

 protected:
  base::ScopedTempDir component_dir_;

 private:
  void OnChromeRootStoreConfigured() override {
    if (crs_config_closure_) {
      std::move(crs_config_closure_).Run();
    }
  }

  void OnMtcMetadataConfigured() override {
    if (mtc_metadata_config_closure_) {
      std::move(mtc_metadata_config_closure_).Run();
    }
  }

  base::OnceClosure crs_config_closure_;
  base::OnceClosure mtc_metadata_config_closure_;
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
                       UpdateTrustAnchorIDs) {
  content::StoragePartition* partition =
      chrome_test_utils::GetActiveWebContents(this)
          ->GetBrowserContext()
          ->GetDefaultStoragePartition();
  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  scoped_refptr<net::X509Certificate> intermediate1 = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "intermediate_ca_cert.pem");
  ASSERT_TRUE(intermediate1);
  scoped_refptr<net::X509Certificate> intermediate2 = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "verisign_intermediate_ca_2016.pem");
  ASSERT_TRUE(intermediate2);

  // Test that the initial set of Trust Anchor IDs comes from the compiled-in
  // root store.
  {
    std::vector<std::vector<uint8_t>> expected_trust_anchor_ids =
        net::TrustStoreChrome::GetTrustAnchorIDsFromCompiledInRootStore();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_THAT(future.Get(),
                testing::UnorderedElementsAreArray(expected_trust_anchor_ids));
  }

  // Install CRS update that contains no trusted Trust Anchor IDs.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    InstallCRSUpdate(std::move(root_store_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_TRUE(future.Get().empty());
  }

  // Install CRS update that contains two trusted Trust Anchor IDs.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->set_trust_anchor_id({0x01, 0x02, 0x03});

    chrome_root_store::TrustAnchor* additional_cert1 =
        root_store_proto.add_additional_certs();
    additional_cert1->set_der(
        std::string(net::x509_util::CryptoBufferAsStringPiece(
            intermediate1->cert_buffer())));
    additional_cert1->set_trust_anchor_id({0x01, 0x02});
    // `additional_cert1`'s trust anchor ID should be ignored because it is not
    // configured as a TLS trust anchor.
    additional_cert1->set_tls_trust_anchor(false);

    chrome_root_store::TrustAnchor* additional_cert2 =
        root_store_proto.add_additional_certs();
    additional_cert2->set_der(
        std::string(net::x509_util::CryptoBufferAsStringPiece(
            intermediate2->cert_buffer())));
    additional_cert2->set_trust_anchor_id({0x02, 0x03});
    additional_cert2->set_tls_trust_anchor(true);

    InstallCRSUpdate(std::move(root_store_proto));

    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();

    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                  std::vector<uint8_t>({0x01, 0x02, 0x3}),
                                  std::vector<uint8_t>({0x02, 0x03})));
  }
}

// Tests that when new network contexts are created after a Trust Anchor IDs
// component update is received, the new network context uses the Trust Anchor
// IDs from the component updater.
IN_PROC_BROWSER_TEST_F(PKIMetadataComponentChromeRootStoreUpdateTest,
                       NewNetworkContextAfterUpdatingTrustAnchorIDs) {
  // This test is only works with an out-of-process network service because it
  // uses a network service crash/restart to test what happens when a new
  // network context is created.
  if (content::IsInProcessNetworkService()) {
    return;
  }

  content::StoragePartition* partition =
      chrome_test_utils::GetActiveWebContents(this)
          ->GetBrowserContext()
          ->GetDefaultStoragePartition();
  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);

  // Install CRS update that contains one trusted Trust Anchor IDs.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->set_trust_anchor_id(
        {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    InstallCRSUpdate(std::move(root_store_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_THAT(future.Get(),
                testing::UnorderedElementsAre(std::vector<uint8_t>(
                    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08})));
  }

  network::mojom::NetworkContext* old_network_context =
      partition->GetNetworkContext();

  // Simulate a network service crash and restart, and check that the newly
  // created network service uses the Trust Anchor ID from the prior component
  // update.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure it notices the crash.
  partition->FlushNetworkInterfaceForTesting();
  {
    // Just to be sure that the test is testing what it intends to, check that a
    // new network context has been created.
    ASSERT_NE(old_network_context, partition->GetNetworkContext());

    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_THAT(future.Get(),
                testing::UnorderedElementsAre(std::vector<uint8_t>(
                    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08})));
  }
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

class PKIMetadataComponentChromeRootStoreMtcMetadataTest
    : public PKIMetadataComponentChromeRootStoreUpdateTest,
      public testing::WithParamInterface<bool> {
 public:
  PKIMetadataComponentChromeRootStoreMtcMetadataTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(net::features::kVerifyMTCs);
    } else {
      feature_list_.InitAndDisableFeature(net::features::kVerifyMTCs);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PKIMetadataComponentChromeRootStoreMtcMetadataTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentChromeRootStoreMtcMetadataTest,
                       TrustAnchorIDsWhenUpdateMtcMetadataBeforeCRS) {
  content::StoragePartition* partition =
      chrome_test_utils::GetActiveWebContents(this)
          ->GetBrowserContext()
          ->GetDefaultStoragePartition();
  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  scoped_refptr<net::X509Certificate> intermediate1 = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "intermediate_ca_cert.pem");
  ASSERT_TRUE(intermediate1);
  scoped_refptr<net::X509Certificate> intermediate2 = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "verisign_intermediate_ca_2016.pem");
  ASSERT_TRUE(intermediate2);

  // Test that the initial set of Trust Anchor IDs comes from the compiled-in
  // root store.
  {
    std::vector<std::vector<uint8_t>> expected_trust_anchor_ids =
        net::TrustStoreChrome::GetTrustAnchorIDsFromCompiledInRootStore();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_THAT(future.Get(),
                testing::UnorderedElementsAreArray(expected_trust_anchor_ids));
  }

  // Install MTC metadata update that contains Trust AnchorIDs for
  // signatureless MTCs. These TAIs should not be used yet since no MTC Anchor
  // has been loaded, however we should see that the regular TAIs are still
  // unchanged even though the MTC Metadata component loaded before the CRS
  // component.
  {
    chrome_root_store::MtcMetadata mtc_metadata_proto;
    mtc_metadata_proto.set_update_time_seconds(
        SecondsSinceEpoch(base::Time::Now()));
    chrome_root_store::MtcAnchorData* mtc_anchor_metadata =
        mtc_metadata_proto.add_mtc_anchor_data();
    mtc_anchor_metadata->set_log_id({0x01, 0x02, 0x03});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()->set_base_id(
        {0x04, 0x05});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_min_active_landmark_inclusive(1);
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_last_landmark_inclusive(2);
    InstallMtcMetadataUpdate(std::move(mtc_metadata_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    // Test that the set of Trust Anchor IDs is still the compiled-in ones.
    std::vector<std::vector<uint8_t>> expected_trust_anchor_ids =
        net::TrustStoreChrome::GetTrustAnchorIDsFromCompiledInRootStore();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_THAT(future.Get(),
                testing::UnorderedElementsAreArray(expected_trust_anchor_ids));
  }

  // Install CRS update that contains a trusted MtcAnchor matching the added
  // MtcMetadata, as well as a traditional anchor with a TAI.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);

    chrome_root_store::MtcAnchor* mtc_anchor =
        root_store_proto.add_mtc_anchors();
    mtc_anchor->set_log_id({0x01, 0x02, 0x03});
    mtc_anchor->set_tls_trust_anchor(true);

    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->set_trust_anchor_id({0x05, 0x06, 0x07});

    InstallCRSUpdate(std::move(root_store_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    if (GetParam()) {
      // Once a Chrome Root Store update containing the matching MtcAnchor is
      // loaded, the signatureless MTC trust anchor IDs should be usable
      // immediately.
      // The current expectation is that a trusted TAI range is represented by
      // the TAI constructed from the base_id + the max landmark number, which
      // implies support for the preceding (non-expired) landmark numbers.
      EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                    std::vector<uint8_t>({0x04, 0x05, 0x02}),
                                    std::vector<uint8_t>({0x05, 0x06, 0x07})));
    } else {
      EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                    std::vector<uint8_t>({0x05, 0x06, 0x07})));
    }
  }
}

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentChromeRootStoreMtcMetadataTest,
                       TrustAnchorIDsWhenUpdateCRSBeforeMtcMetadata) {
  content::StoragePartition* partition =
      chrome_test_utils::GetActiveWebContents(this)
          ->GetBrowserContext()
          ->GetDefaultStoragePartition();
  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  scoped_refptr<net::X509Certificate> intermediate1 = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "intermediate_ca_cert.pem");
  ASSERT_TRUE(intermediate1);
  scoped_refptr<net::X509Certificate> intermediate2 = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "verisign_intermediate_ca_2016.pem");
  ASSERT_TRUE(intermediate2);

  // Test that the initial set of Trust Anchor IDs comes from the compiled-in
  // root store.
  {
    std::vector<std::vector<uint8_t>> expected_trust_anchor_ids =
        net::TrustStoreChrome::GetTrustAnchorIDsFromCompiledInRootStore();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_THAT(future.Get(),
                testing::UnorderedElementsAreArray(expected_trust_anchor_ids));
  }

  // Install CRS update that contains a trusted MtcAnchor matching the
  // MtcMetadata which will be added later, as well as a traditional anchor
  // with a TAI.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);

    chrome_root_store::MtcAnchor* mtc_anchor =
        root_store_proto.add_mtc_anchors();
    mtc_anchor->set_log_id({0x01, 0x02, 0x03});
    mtc_anchor->set_tls_trust_anchor(true);

    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->set_trust_anchor_id({0x05, 0x06, 0x07});

    InstallCRSUpdate(std::move(root_store_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    // Only the TAI from the traditional anchor is available, since the MTC
    // Metadata hasn't loaded yet.
    EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                  std::vector<uint8_t>({0x05, 0x06, 0x07})));
  }

  // Install MTC metadata update that contains Trust AnchorIDs for
  // signatureless MTCs. Since the MTC Anchor is already loaded, the
  // signatureless TAIs should be used immediately.
  {
    chrome_root_store::MtcMetadata mtc_metadata_proto;
    mtc_metadata_proto.set_update_time_seconds(
        SecondsSinceEpoch(base::Time::Now()));
    chrome_root_store::MtcAnchorData* mtc_anchor_metadata =
        mtc_metadata_proto.add_mtc_anchor_data();
    mtc_anchor_metadata->set_log_id({0x01, 0x02, 0x03});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()->set_base_id(
        {0x04, 0x05});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_min_active_landmark_inclusive(1);
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_last_landmark_inclusive(2);
    InstallMtcMetadataUpdate(std::move(mtc_metadata_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    if (GetParam()) {
      // The current expectation is that a trusted TAI range is represented by
      // the TAI constructed from the base_id + the max landmark number, which
      // implies support for the preceding (non-expired) landmark numbers.
      EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                    std::vector<uint8_t>({0x04, 0x05, 0x02}),
                                    std::vector<uint8_t>({0x05, 0x06, 0x07})));
    } else {
      EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                    std::vector<uint8_t>({0x05, 0x06, 0x07})));
    }
  }
}

IN_PROC_BROWSER_TEST_P(PKIMetadataComponentChromeRootStoreMtcMetadataTest,
                       StaleMtcMetadata) {
  content::StoragePartition* partition =
      chrome_test_utils::GetActiveWebContents(this)
          ->GetBrowserContext()
          ->GetDefaultStoragePartition();
  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  scoped_refptr<net::X509Certificate> intermediate1 = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "intermediate_ca_cert.pem");
  ASSERT_TRUE(intermediate1);
  scoped_refptr<net::X509Certificate> intermediate2 = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "verisign_intermediate_ca_2016.pem");
  ASSERT_TRUE(intermediate2);

  // Test that the initial set of Trust Anchor IDs comes from the compiled-in
  // root store.
  {
    std::vector<std::vector<uint8_t>> expected_trust_anchor_ids =
        net::TrustStoreChrome::GetTrustAnchorIDsFromCompiledInRootStore();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    EXPECT_THAT(future.Get(),
                testing::UnorderedElementsAreArray(expected_trust_anchor_ids));
  }

  // Install CRS update that contains a trusted MtcAnchor matching the
  // MtcMetadata which will be added later, as well as a traditional anchor
  // with a TAI.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);

    chrome_root_store::MtcAnchor* mtc_anchor =
        root_store_proto.add_mtc_anchors();
    mtc_anchor->set_log_id({0x01, 0x02, 0x03});
    mtc_anchor->set_tls_trust_anchor(true);

    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));
    anchor->set_trust_anchor_id({0x05, 0x06, 0x07});

    InstallCRSUpdate(std::move(root_store_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    // Only the TAI from the traditional anchor is available, since the MTC
    // Metadata hasn't loaded yet.
    EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                  std::vector<uint8_t>({0x05, 0x06, 0x07})));
  }

  // Attempt to install MTC metadata update that contains Trust AnchorIDs for
  // signatureless MTCs, but which has an out-of-date update time. It should be
  // ignored.
  {
    chrome_root_store::MtcMetadata mtc_metadata_proto;
    mtc_metadata_proto.set_update_time_seconds(
        SecondsSinceEpoch(base::Time::Now() - base::Days(14)));
    chrome_root_store::MtcAnchorData* mtc_anchor_metadata =
        mtc_metadata_proto.add_mtc_anchor_data();
    mtc_anchor_metadata->set_log_id({0x01, 0x02, 0x03});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()->set_base_id(
        {0x04, 0x05});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_min_active_landmark_inclusive(1);
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_last_landmark_inclusive(2);
    InstallMtcMetadataUpdate(std::move(mtc_metadata_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    // MTCMetadata update should have been ignored, so the TAI will still be
    // only from the traditional anchor.
    EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                  std::vector<uint8_t>({0x05, 0x06, 0x07})));
  }

  // Install a new MTC metadata update that contains Trust AnchorIDs for
  // signatureless MTCs and which is up to date.
  {
    chrome_root_store::MtcMetadata mtc_metadata_proto;
    mtc_metadata_proto.set_update_time_seconds(
        SecondsSinceEpoch(base::Time::Now() - base::Days(1)));
    chrome_root_store::MtcAnchorData* mtc_anchor_metadata =
        mtc_metadata_proto.add_mtc_anchor_data();
    mtc_anchor_metadata->set_log_id({0x01, 0x02, 0x03});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()->set_base_id(
        {0x04, 0x05});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_min_active_landmark_inclusive(1);
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_last_landmark_inclusive(4);
    InstallMtcMetadataUpdate(std::move(mtc_metadata_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    if (GetParam()) {
      EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                    std::vector<uint8_t>({0x04, 0x05, 0x04}),
                                    std::vector<uint8_t>({0x05, 0x06, 0x07})));
    } else {
      EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                    std::vector<uint8_t>({0x05, 0x06, 0x07})));
    }
  }

  // Attempt to install another stale MTC metadata update that contains Trust
  // AnchorIDs for signatureless MTCs. It should be ignored.
  {
    chrome_root_store::MtcMetadata mtc_metadata_proto;
    mtc_metadata_proto.set_update_time_seconds(
        SecondsSinceEpoch(base::Time::Now() - base::Days(13)));
    chrome_root_store::MtcAnchorData* mtc_anchor_metadata =
        mtc_metadata_proto.add_mtc_anchor_data();
    mtc_anchor_metadata->set_log_id({0x01, 0x02, 0x03});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()->set_base_id(
        {0x04, 0x05});
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_min_active_landmark_inclusive(1);
    mtc_anchor_metadata->mutable_trusted_landmark_ids_range()
        ->set_last_landmark_inclusive(3);
    InstallMtcMetadataUpdate(std::move(mtc_metadata_proto));
    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    partition->GetNetworkContext()->GetTrustAnchorIDsForTesting(
        future.GetCallback());
    // This MTCMetadata update should have been ignored, so the TAI will still
    // be from the previous successful update. (This is slightly weird test
    // scenario since you wouldn't normally expect to have a still-valid
    // component and then be served an older, out-of-date one.)
    if (GetParam()) {
      EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                    std::vector<uint8_t>({0x04, 0x05, 0x04}),
                                    std::vector<uint8_t>({0x05, 0x06, 0x07})));
    } else {
      EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(
                                    std::vector<uint8_t>({0x05, 0x06, 0x07})));
    }
  }
}

// TODO(crbug.com/452986180): test end-to-end MTC verification

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
    case CTEnforcement::kEnabledWithOne6962Enforcement:
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
    case CTEnforcement::kEnabledWithOne6962Enforcement:
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
                    CTEnforcement::kEnabledWithOne6962Enforcement,
                    CTEnforcement::kDisabledByProto,
                    CTEnforcement::kDisabledByFeature));

std::string X509CertificateToString(scoped_refptr<net::X509Certificate> cert) {
  std::vector<std::string> pem_encoded_chain;
  EXPECT_TRUE(cert->GetPEMEncodedChain(&pem_encoded_chain));
  return base::JoinString(pem_encoded_chain, "\n");
}

// Checks that navigation responses were served over a connection where the
// server provided the given `expected_server_certificate_chain`. Note that this
// checks the certificate chain that the server served, not the chain that the
// client built while validating the server's certificate.
class CertificateCheckingThrottle : public content::NavigationThrottle {
 public:
  CertificateCheckingThrottle(
      content::NavigationThrottleRegistry& registry,
      scoped_refptr<net::X509Certificate> expected_server_certificate_chain,
      base::OnceCallback<void(uint8_t)> report_num_responses_callback)
      : content::NavigationThrottle(registry),
        expected_server_certificate_chain_(expected_server_certificate_chain),
        report_num_responses_callback_(
            std::move(report_num_responses_callback)) {}

  CertificateCheckingThrottle(const CertificateCheckingThrottle&) = delete;
  CertificateCheckingThrottle& operator=(const CertificateCheckingThrottle&) =
      delete;
  ~CertificateCheckingThrottle() override {
    std::move(report_num_responses_callback_).Run(num_responses_);
  }

  uint8_t num_responses() const { return num_responses_; }

 protected:
  const char* GetNameForLogging() override {
    return "CertificateCheckingThrottle";
  }

  ThrottleCheckResult WillProcessResponse() override {
    EXPECT_TRUE(navigation_handle()
                    ->GetSSLInfo()
                    ->unverified_cert->EqualsIncludingChain(
                        expected_server_certificate_chain_.get()))
        << "\n\nExpected server chain: "
        << X509CertificateToString(expected_server_certificate_chain_)
        << "\n\nObserved unverified server chain: "
        << X509CertificateToString(
               navigation_handle()->GetSSLInfo()->unverified_cert);
    ++num_responses_;
    return content::NavigationThrottle::PROCEED;
  }

 private:
  scoped_refptr<net::X509Certificate> expected_server_certificate_chain_;
  uint8_t num_responses_ = 0;
  base::OnceCallback<void(uint8_t)> report_num_responses_callback_;
};

class TestDnsOverHttpsConfigSource : public DnsOverHttpsConfigSource {
 public:
  TestDnsOverHttpsConfigSource(std::string dns_over_https_templates,
                               std::string dns_over_https_mode)
      : dns_over_https_templates_(std::move(dns_over_https_templates)),
        dns_over_https_mode_(std::move(dns_over_https_mode)) {}

  TestDnsOverHttpsConfigSource(const TestDnsOverHttpsConfigSource&) = delete;
  TestDnsOverHttpsConfigSource& operator=(const TestDnsOverHttpsConfigSource&) =
      delete;
  ~TestDnsOverHttpsConfigSource() override = default;

  // DnsOverHttpsConfigSource:
  std::string GetDnsOverHttpsMode() const override {
    return dns_over_https_mode_;
  }
  std::string GetDnsOverHttpsTemplates() const override {
    return dns_over_https_templates_;
  }
  bool AutomaticModeFallbackToDohEnabled() const override { return false; }
  bool IsConfigManaged() const override {
    // Return managed=true, otherwise the test config will be ignored if the
    // test is run on an enterprise enrolled device.
    return true;
  }
  void SetDohChangeCallback(base::RepeatingClosure callback) override {}

 private:
  std::string dns_over_https_templates_;
  std::string dns_over_https_mode_;
};

// Test fixture for testing Trust Anchor IDs, including a test DoH server for
// advertising Trust Anchor IDs in DNS.
class PKIMetadataComponentChromeRootStoreUpdateWithDoHServerTest
    : public PKIMetadataComponentChromeRootStoreUpdateTest {
 public:
  static constexpr std::string_view kDohServerHostname = "doh.test";
  static constexpr std::string_view kHostname = "a.com";

  PKIMetadataComponentChromeRootStoreUpdateWithDoHServerTest()
      : PKIMetadataComponentChromeRootStoreUpdateTest() {
    feature_list_.InitAndEnableFeature(net::features::kTLSTrustAnchorIDs);
  }

  void SetUpOnMainThread() override {
    // Set up an HTTPS server that has two certificate chains.
    // The first is directly issued by a unique root with the trust anchor ID
    // `kIntermediateTrustAnchorId`.
    // The second chain has a leaf and intermediate issued by the default test
    // root cert, and has no trust anchor ID.
    net::SSLServerConfig server_config;
    // TODO(crbug.com/431064813): this callback just adds some debugging
    // info to try to investigate a flake. It can be removed once the cause of
    // the flake is found.
    server_config.client_hello_callback_for_testing =
        base::BindLambdaForTesting([&](const SSL_CLIENT_HELLO* client_hello) {
          const uint8_t* data = nullptr;
          size_t len = 0;
          SSL_early_callback_ctx_extension_get(
              client_hello, TLSEXT_TYPE_trust_anchors, &data, &len);
          LOG(ERROR) << "Trust anchor IDs from Client Hello: "
                     << base::HexEncode(data, len);
          return true;
        });

    net::EmbeddedTestServer::ServerCertificateConfig tai_cert_config;
    tai_cert_config.intermediate =
        net::EmbeddedTestServer::IntermediateType::kNone;
    tai_cert_config.root = net::EmbeddedTestServer::RootType::kUniqueRoot;
    tai_cert_config.trust_anchor_id =
        base::ToVector(kIntermediateTrustAnchorId);
    tai_cert_config.dns_names.emplace_back(kHostname);

    net::EmbeddedTestServer::ServerCertificateConfig default_cert_config;
    default_cert_config.intermediate =
        net::EmbeddedTestServer::IntermediateType::kInHandshake;
    default_cert_config.root = net::EmbeddedTestServer::RootType::kUniqueRoot;
    default_cert_config.dns_names.emplace_back(kHostname);

    trust_anchor_ids_server_.SetSSLConfig(
        {tai_cert_config, default_cert_config}, server_config);
    trust_anchor_ids_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(trust_anchor_ids_server_.Start());

    // Start a DoH server, which ensures we use a resolver with HTTPS RR
    // support. Configure it to serve records for `trust_anchor_ids_server_`.
    doh_server_.SetHostname(kDohServerHostname);
    url::SchemeHostPort tai_host(
        trust_anchor_ids_server_.GetURL(kHostname, "/"));
    doh_server_.AddAddressRecord(tai_host.host(),
                                 net::IPAddress::IPv4Localhost());
    doh_server_.AddRecord(net::BuildTestHttpsServiceRecord(
        net::dns_util::GetNameForHttpsQuery(tai_host),
        /*priority=*/1, /*service_name=*/tai_host.host(),
        {net::BuildTestHttpsServiceTrustAnchorIDsParam(
            GetTrustAnchorIDsForDns())}));
    ASSERT_TRUE(doh_server_.Start());

    doh_config_source_ = std::make_unique<TestDnsOverHttpsConfigSource>(
        doh_server_.GetTemplate(), SecureDnsConfig::kModeSecure);
    SystemNetworkContextManager::GetStubResolverConfigReader()
        ->SetOverrideDnsOverHttpsConfigSource(std::move(doh_config_source_));
    // The net stack doesn't enable DoH when it can't find a system DNS config
    // (see https://crbug.com/1251715).
    SetReplaceSystemDnsConfig();

    // Ensure that the DoH configuration is picked up.
    content::FlushNetworkServiceInstanceForTesting();

    // Add a single bootstrapping rule so we can resolve the DoH server.
    host_resolver()->AddRule(kDohServerHostname, "127.0.0.1");
  }

  void UpdateNumObservedResponses(uint8_t num_responses) {
    num_observed_responses_ += num_responses;
  }

 protected:
  // The Trust Anchor ID configured by `trust_anchor_ids_server_` for the
  // intermediate that it uses in its certificate chain.
  static constexpr uint8_t kIntermediateTrustAnchorId[] = {0x01, 0x02, 0x03};

  // A Trust Anchor ID that is advertised for `trust_anchor_ids_server_` in DNS,
  // but not actually associated with a certificate chain configured on the
  // server.
  static constexpr uint8_t kAdvertisedButNotServedTrustAnchorId[] = {0x04, 0x05,
                                                                     0x06};
  // A Trust Anchor ID that is neither advertised for `trust_anchor_ids_server_`
  // in DNS, nor actually associated with a certificate chain configured on the
  // server.
  static constexpr uint8_t kNotAdvertisedAndNotServedTrustAnchorId[] = {
      0x07, 0x08, 0x09};

  static constexpr size_t kTaiCredentialNum = 0;
  static constexpr size_t kDefaultCredentialNum = 1;

  // By default, `kIntermediateTrustAnchorId` and
  // `kAdvertisedButNotServedTrustAnchorId` are advertised for `kHostname` in an
  // HTTPS record served by `doh_server_`. Subclasses can override this method
  // to change which Trust Anchor IDs are advertised for this host.
  virtual std::vector<std::vector<uint8_t>> GetTrustAnchorIDsForDns() {
    return {base::ToVector(kAdvertisedButNotServedTrustAnchorId),
            base::ToVector(kIntermediateTrustAnchorId)};
  }

  // Installs a navigation throttle that expects `certificate` to be the served
  // certificate chain on successful responses. Overwrites previous calls to
  // this method (i.e., only one certificate-checking throttle is in place at a
  // time). When the navigation is finished and the inserted throttle is
  // destroyed, UpdateNumObservedResponses() will be called, which allows tests
  // to check that the throttle was successfully installed and observed a
  // navigation.
  void SetExpectedCertificateOnResponses(
      scoped_refptr<net::X509Certificate> certificate) {
    num_observed_responses_ = 0;
    throttle_inserter_ =
        std::make_unique<content::TestNavigationThrottleInserter>(
            chrome_test_utils::GetActiveWebContents(this),
            base::BindRepeating(
                &PKIMetadataComponentChromeRootStoreUpdateWithDoHServerTest::
                    InsertThrottle,
                base::Unretained(this), certificate));
  }

  void InsertThrottle(
      scoped_refptr<net::X509Certificate> expected_server_certificate,
      content::NavigationThrottleRegistry& registry) {
    registry.AddThrottle(std::make_unique<CertificateCheckingThrottle>(
        registry, expected_server_certificate,
        base::BindOnce(
            &PKIMetadataComponentChromeRootStoreUpdateWithDoHServerTest::
                UpdateNumObservedResponses,
            base::Unretained(this))));
  }

  // Checks that the most recently installed navigation throttle observed at
  // least one response.
  void CheckThrottleObservedNavigation() {
    ASSERT_GT(num_observed_responses_, 0u);
  }

  net::EmbeddedTestServer trust_anchor_ids_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
  net::TestDohServer doh_server_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::TestNavigationThrottleInserter> throttle_inserter_;
  std::unique_ptr<TestDnsOverHttpsConfigSource> doh_config_source_;
  // Tracks the number of responses observed by CertificateCheckingThrottles.
  // Reset to 0 on each new `SetExpectedCertificateOnResponses()` call.
  uint8_t num_observed_responses_ = 0;
};

IN_PROC_BROWSER_TEST_F(
    PKIMetadataComponentChromeRootStoreUpdateWithDoHServerTest,
    TrustAnchorIDs) {
  int64_t crs_version = net::CompiledChromeRootStoreVersion();

  {
    // Install CRS update that contains only the default root and no trust
    // anchor ids.
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(net::x509_util::CryptoBufferAsStringPiece(
        trust_anchor_ids_server_.GetRoot(kDefaultCredentialNum)
            ->cert_buffer())));

    InstallCRSUpdate(std::move(root_store_proto));

    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();

    // Before updating the root store with trust anchor IDs, the server should
    // serve the default credential which has both a leaf and an intermediate.
    scoped_refptr<net::X509Certificate> server_certificate =
        trust_anchor_ids_server_.GetCertificate(kDefaultCredentialNum);
    ASSERT_EQ(server_certificate->intermediate_buffers().size(), 1u);

    SetExpectedCertificateOnResponses(server_certificate);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), trust_anchor_ids_server_.GetURL(kHostname, "/simple.html")));
    ASSERT_EQ(chrome_test_utils::GetActiveWebContents(this)->GetTitle(), u"OK");
    CheckThrottleObservedNavigation();
  }

  // Install CRS update that contains two trusted Trust Anchor IDs, including
  // one that is advertised by the server corresponding to its root
  // certificate.
  {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++crs_version);
    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    anchor->set_der(std::string(net::x509_util::CryptoBufferAsStringPiece(
        trust_anchor_ids_server_.GetRoot(kDefaultCredentialNum)
            ->cert_buffer())));

    chrome_root_store::TrustAnchor* additional_cert1 =
        root_store_proto.add_additional_certs();
    additional_cert1->set_der(
        std::string(net::x509_util::CryptoBufferAsStringPiece(
            trust_anchor_ids_server_.GetRoot(kTaiCredentialNum)
                ->cert_buffer())));
    additional_cert1->set_trust_anchor_id(
        base::as_string_view(kIntermediateTrustAnchorId));
    additional_cert1->set_tls_trust_anchor(true);

    chrome_root_store::TrustAnchor* additional_cert2 =
        root_store_proto.add_additional_certs();
    scoped_refptr<net::X509Certificate> unused_intermediate =
        net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                "verisign_intermediate_ca_2016.pem");
    additional_cert2->set_der(
        std::string(net::x509_util::CryptoBufferAsStringPiece(
            unused_intermediate->cert_buffer())));
    additional_cert2->set_trust_anchor_id(
        base::as_string_view(kNotAdvertisedAndNotServedTrustAnchorId));
    additional_cert2->set_tls_trust_anchor(true);

    InstallCRSUpdate(std::move(root_store_proto));

    // Ensure that SSLConfigClients have been notified of the new trust anchor
    // IDs.
    SystemNetworkContextManager::GetInstance()
        ->FlushSSLConfigManagerForTesting();

    // The server should now serve a single leaf, without any intermediates,
    // because the client should signal that it trusts the intermediate as a
    // trust anchor.
    scoped_refptr<net::X509Certificate> server_certificate =
        trust_anchor_ids_server_.GetCertificate(kTaiCredentialNum);
    ASSERT_EQ(server_certificate->intermediate_buffers().size(), 0u);
    SetExpectedCertificateOnResponses(server_certificate);

    // TODO(crbug.com/431064813): remove after debugging test flake.
    LOG(ERROR) << "Beginning navigation with Trust Anchor IDs";

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), trust_anchor_ids_server_.GetURL(kHostname, "/simple.html")));
    ASSERT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
    CheckThrottleObservedNavigation();
  }
}

// Test fixture that simulates a stale DNS record, advertising a Trust Anchor ID
// that is not supported by the server. The root store does not trust the root
// for the full chain served by the server and accepts only an elided chain,
// for testing the Trust Anchor IDs retry flow.
class PKIMetadataComponentChromeRootStoreUpdateWithStaleDoHServerTest
    : public PKIMetadataComponentChromeRootStoreUpdateWithDoHServerTest {
 public:
  PKIMetadataComponentChromeRootStoreUpdateWithStaleDoHServerTest() = default;

 protected:
  std::vector<std::vector<uint8_t>> GetTrustAnchorIDsForDns() override {
    return {base::ToVector(kAdvertisedButNotServedTrustAnchorId)};
  }
};

IN_PROC_BROWSER_TEST_F(
    PKIMetadataComponentChromeRootStoreUpdateWithStaleDoHServerTest,
    TrustAnchorIDsRetry) {
  // Install CRS update that contains two trusted Trust Anchor IDs, including
  // one that is advertised by the server corresponding to its intermediate
  // certificate, and one that is advertised by the server but not actually used
  // on the server (simulating, e.g., a stale DNS record that is out of sync
  // with the server's actual credentials). The CRS update does NOT trust the
  // test server's default (non-TAI) root.
  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  chrome_root_store::RootStore root_store_proto;
  root_store_proto.set_version_major(++crs_version);

  chrome_root_store::TrustAnchor* additional_cert1 =
      root_store_proto.add_additional_certs();
  additional_cert1->set_der(
      std::string(net::x509_util::CryptoBufferAsStringPiece(
          trust_anchor_ids_server_.GetRoot(kTaiCredentialNum)->cert_buffer())));
  additional_cert1->set_trust_anchor_id(
      base::as_string_view(kIntermediateTrustAnchorId));
  additional_cert1->set_tls_trust_anchor(true);

  chrome_root_store::TrustAnchor* additional_cert2 =
      root_store_proto.add_additional_certs();
  scoped_refptr<net::X509Certificate> unused_intermediate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "verisign_intermediate_ca_2016.pem");
  ASSERT_TRUE(unused_intermediate);
  additional_cert2->set_der(
      std::string(net::x509_util::CryptoBufferAsStringPiece(
          unused_intermediate->cert_buffer())));
  additional_cert2->set_trust_anchor_id(
      base::as_string_view(kAdvertisedButNotServedTrustAnchorId));
  additional_cert2->set_tls_trust_anchor(true);

  InstallCRSUpdate(std::move(root_store_proto));

  // Ensure that SSLConfigClients have been notified of the new trust anchor
  // IDs.
  SystemNetworkContextManager::GetInstance()->FlushSSLConfigManagerForTesting();

  // Send a request to the server. Initially, the client will advertise the
  // intersection of what is advertised in DNS with its trust store -- i.e.,
  // only `kAdvertisedButNotServedTrustAnchorID`. The server does not actually
  // support this Trust Anchor ID, and thus will serve its full chain. This
  // should result in a certificate error (since kDefaultCredentialNum root is
  // not trusted), which will cause the client to retry using the Trust Anchor
  // ID that the server actually supports. The final result is that the
  // connection should succeed and serve the elided certificate chain.
  SetExpectedCertificateOnResponses(
      trust_anchor_ids_server_.GetCertificate(kTaiCredentialNum));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), trust_anchor_ids_server_.GetURL(kHostname, "/simple.html")));
  ASSERT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  CheckThrottleObservedNavigation();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // This test uses ExpectBucketCount rather than ExpectUniqueSample, since
  // other results are likely to be recorded by the histogram during the
  // navigation. The DoH lookups should record kNoDnsSuccessInitial.
  // The connection done by the test should be the only one that has a
  // possibility of recording kDnsSuccessRetry, so this will still verify that
  // the test hit the expected result. After the connection is successful
  // another entry may be recorded for the favicon fetch, but it should record
  // kDnsSuccessInitial since it will use TLS session resumption.
  //
  // Sometimes (on builds where browser_tests isn't using
  // fieldtrial_testing_config), the browser makes two connections. So just
  // check that the bucket has been logged at least once.
  EXPECT_GE(histogram_tester.GetBucketCount(
                "Net.SSL.TrustAnchorIDsResult",
                net::SSLClientSocket::TrustAnchorIDsResult::kDnsSuccessRetry),
            1);

  // TODO(crbug.com/427778127): when Trust Anchor ID netlogs are added, check
  // them here.
}

// TODO(crbug.com/40816087) additional Chrome Root Store browser tests to
// add:
//
// * Test that AIA fetching still works after updating CRS.
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

}  // namespace component_updater
