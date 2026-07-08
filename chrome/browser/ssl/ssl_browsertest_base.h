// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_SSL_SSL_BROWSERTEST_BASE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/security_interstitials/core/controller_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

namespace content {
class WebContents;
}

namespace security_interstitials {
class SecurityInterstitialPage;
class SecurityInterstitialControllerClient;
}  // namespace security_interstitials

class SSLBlockingPage;
class Browser;

class SSLUITestBase : public InProcessBrowserTest,
                      public network::mojom::SSLConfigClient {
 public:
  SSLUITestBase();
  SSLUITestBase(const SSLUITestBase&) = delete;
  SSLUITestBase& operator=(const SSLUITestBase&) = delete;
  ~SSLUITestBase() override;

  // InProcessBrowserTest:
  void SetUp() override;
  void TearDown() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // network::mojom::SSLConfigClient:
  void OnSSLConfigUpdated(network::mojom::SSLConfigPtr config) override;

  static std::string GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair);

 protected:
  // Returns disabled features that subclasses may choose to use with
  // ScopedFeatureList.
  static std::vector<base::test::FeatureRef> GetDisabledFeatures();

  static std::string GetTopFramePath(
      const net::EmbeddedTestServer& http_server,
      const net::EmbeddedTestServer& good_https_server,
      const net::EmbeddedTestServer& bad_https_server);

  void ProceedThroughInterstitial(content::WebContents* tab);
  virtual void DontProceedThroughInterstitial(content::WebContents* tab);

  void SendInterstitialCommand(
      content::WebContents* tab,
      security_interstitials::SecurityInterstitialCommand command);

  // Navigates to an interstitial and clicks through the certificate
  // error; then navigates to a page at |path| that loads unsafe content.
  void SetUpUnsafeContentsWithUserException(const std::string& path);

  void UpdateChromePolicy(const policy::PolicyMap& policies);

  void RunOnIOThreadBlocking(base::OnceClosure task);

  security_interstitials::SecurityInterstitialPage* GetInterstitialPage(
      content::WebContents* tab);

  // Helper function for TestInterstitialLinksOpenInNewTab. Implemented as a
  // test fixture method because the whole test fixture class is friended by
  // SSLBlockingPage.
  security_interstitials::SecurityInterstitialControllerClient*
  GetControllerClientFromSSLBlockingPage(SSLBlockingPage* ssl_interstitial);

  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer https_server_expired_;
  net::EmbeddedTestServer https_server_mismatched_;
  net::EmbeddedTestServer https_server_sha1_;
  net::EmbeddedTestServer https_server_common_name_only_;
  net::EmbeddedTestServer wss_server_expired_;
  net::EmbeddedTestServer wss_server_mismatched_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  network::mojom::SSLConfig last_ssl_config_;
  mojo::Receiver<network::mojom::SSLConfigClient> receiver_{this};

 private:
  network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams();
};

#endif  // CHROME_BROWSER_SSL_SSL_BROWSERTEST_BASE_H_
