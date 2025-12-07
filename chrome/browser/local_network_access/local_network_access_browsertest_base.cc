// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace local_network_access {

LocalNetworkAccessBrowserTestBase::LocalNetworkAccessBrowserTestBase(
    bool map_all_hosts_to_localhost)
    : map_all_hosts_to_localhost_(map_all_hosts_to_localhost),
      https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
  // Some builders run with field_trial disabled, need to enable this
  // manually.
  base::FieldTrialParams params;
  params["LocalNetworkAccessChecksWarn"] = "false";
  features_.InitAndEnableFeatureWithParameters(
      network::features::kLocalNetworkAccessChecks, params);
}

LocalNetworkAccessBrowserTestBase::~LocalNetworkAccessBrowserTestBase() =
    default;

// Fetch the Blink.UseCounter.Features histogram in every renderer process
// until reaching, but not exceeding, |expected_count|.
void LocalNetworkAccessBrowserTestBase::CheckCounter(WebFeature feature,
                                                     int expected_count) {
  CheckHistogramCount("Blink.UseCounter.Features", feature, expected_count);
}

// Fetch the |histogram|'s |bucket| in every renderer process until reaching,
// but not exceeding, |expected_count|.
template <typename T>
void LocalNetworkAccessBrowserTestBase::CheckHistogramCount(
    std::string_view histogram,
    T bucket,
    int expected_count) {
  while (true) {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    int count = histogram_.GetBucketCount(histogram, bucket);
    CHECK_LE(count, expected_count);
    if (count == expected_count) {
      return;
    }

    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1));
    run_loop.Run();
  }
}

permissions::PermissionRequestManager*
LocalNetworkAccessBrowserTestBase::GetPermissionRequestManager() {
  return permissions::PermissionRequestManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
}

permissions::MockPermissionPromptFactory*
LocalNetworkAccessBrowserTestBase::bubble_factory() {
  return mock_permission_prompt_factory_.get();
}

void LocalNetworkAccessBrowserTestBase::SetUpOnMainThread() {
  permissions::PermissionRequestManager* manager =
      GetPermissionRequestManager();
  mock_permission_prompt_factory_ =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);
  if (map_all_hosts_to_localhost_) {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
}

void LocalNetworkAccessBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Ignore cert errors when connecting to https_server()
  command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  // Clear default from InProcessBrowserTest as test doesn't want 127.0.0.1 in
  // the public address space
  command_line->AppendSwitchASCII(network::switches::kIpAddressSpaceOverrides,
                                  "");
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());
}

}  // namespace local_network_access
