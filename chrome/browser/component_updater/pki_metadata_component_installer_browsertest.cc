// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
#include "base/command_line.h"
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
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

enum class CTEnforcement { kEnabled, kDisabled };

void SetRequireCTForTesting() {
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterface(
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
    CHECK(component_dir_.CreateUniqueTempDir());

    // Set up a configuration that will enable or disable CT enforcement
    // depending on the test parameter.
    chrome_browser_certificate_transparency::CTConfig ct_config;
    ct_config.set_disable_ct_enforcement(GetParam() ==
                                         CTEnforcement::kDisabled);
    ct_config.mutable_log_list()->mutable_timestamp()->set_seconds(
        (base::Time::Now() - base::Time::UnixEpoch()).InSeconds());
    PKIMetadataComponentInstallerService::GetInstance()
        ->WriteComponentForTesting(component_dir_.GetPath(),
                                   ct_config.SerializeAsString());
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
  void OnPKIMetadataConfigured() override {
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
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetMainFrame()));
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
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetMainFrame()));
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

}  // namespace component_updater
