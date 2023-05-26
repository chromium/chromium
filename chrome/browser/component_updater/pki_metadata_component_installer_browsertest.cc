// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/component_updater/pki_metadata_component_installer.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/certificate_transparency/certificate_transparency_config.pb.h"
#include "components/certificate_transparency/ct_features.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#endif

namespace {

enum class CTEnforcement { kEnabled, kDisabled };

void SetRequireCTForTesting() {
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());

  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test->SetRequireCT(
      network::mojom::NetworkServiceTest::RequireCT::REQUIRE);
  return;
}

}  // namespace

namespace component_updater {

// TODO(crbug.com/1286121): add tests for pinning enforcement.
class PKIMetadataComponentUpdaterTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<CTEnforcement>,
      public PKIMetadataComponentInstallerService::Observer {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PKIMetadataComponentInstallerService::GetInstance()->AddObserver(this);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
    ASSERT_TRUE(component_dir_.CreateUniqueTempDir());

    // Set up a configuration that will enable or disable CT enforcement
    // depending on the test parameter.
    chrome_browser_certificate_transparency::CTConfig ct_config;
    ct_config.set_disable_ct_enforcement(GetParam() ==
                                         CTEnforcement::kDisabled);
    ct_config.mutable_log_list()->mutable_timestamp()->set_seconds(
        (base::Time::Now() - base::Time::UnixEpoch()).InSeconds());
    ASSERT_TRUE(PKIMetadataComponentInstallerService::GetInstance()
                    ->WriteCTDataForTesting(component_dir_.GetPath(),
                                            ct_config.SerializeAsString()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    PKIMetadataComponentInstallerService::GetInstance()->RemoveObserver(this);
  }

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }

  // Waits for the PKI to have been configured at least |expected_times|.
  void WaitForPKIConfiguration(int expected_times) {
    expected_pki_metadata_configured_times_ = expected_times;
    if (pki_metadata_configured_times_ >=
        expected_pki_metadata_configured_times_) {
      return;
    }
    base::RunLoop run_loop;
    pki_metadata_config_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnCTLogListConfigured() override {
    ++pki_metadata_configured_times_;
    if (pki_metadata_config_closure_ &&
        pki_metadata_configured_times_ >=
            expected_pki_metadata_configured_times_) {
      std::move(pki_metadata_config_closure_).Run();
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      certificate_transparency::features::
          kCertificateTransparencyComponentUpdater};
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
  if (!content::IsOutOfProcessNetworkService())
    return;

  // CT enforcement is disabled by default on tests. Override this behaviour.
  SetRequireCTForTesting();
  WaitForPKIConfiguration(1);

  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_ok.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("/simple.html")));

  // Check that the page is blocked depending on CT enforcement.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  if (GetParam() == CTEnforcement::kEnabled) {
    EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  } else {
    EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  }

  // Restart the network service.
  SimulateNetworkServiceCrash();
  SetRequireCTForTesting();
  WaitForPKIConfiguration(2);

  // Check that the page is still blocked depending on CT enforcement.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_ok.GetURL("/simple.html")));
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetPrimaryMainFrame()));
  if (GetParam() == CTEnforcement::kEnabled) {
    EXPECT_NE(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  } else {
    EXPECT_EQ(u"OK", chrome_test_utils::GetActiveWebContents(this)->GetTitle());
  }
}

INSTANTIATE_TEST_SUITE_P(PKIMetadataComponentUpdater,
                         PKIMetadataComponentUpdaterTest,
                         testing::Values(CTEnforcement::kEnabled,
                                         CTEnforcement::kDisabled));

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
        absl::nullopt);
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

  void InstallCRSUpdate(const std::vector<std::string>& der_roots) {
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(++last_used_crs_version_);
    for (const auto& der_root : der_roots) {
      root_store_proto.add_trust_anchors()->set_der(der_root);
    }

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

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }

  base::ScopedTempDir component_dir_;

 private:
  void OnChromeRootStoreConfigured() override {
    if (crs_config_closure_) {
      std::move(crs_config_closure_).Run();
    }
  }

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  base::test::ScopedFeatureList scoped_feature_list_{
      net::features::kChromeRootStoreUsed};
#endif

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

  constexpr char kHostname[] = "a.example.com";

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

// TODO(https://crbug.com/1287211) additional Chrome Root Store browser tests to
// add:
//
// * Test that AIA fetching still works after updating CRS.
// * Test with the kChromeRootStoreUsed feature disabled: configuring a CRS
//   update with the test root should not cause the page to load successfully.
// * Test that updates propagate into TrialComparisonCertVerifier too. Testing
//   that loading the root in CRS would cause it to succeed with the trial
//   verifier but not with primary.
#endif

}  // namespace component_updater
