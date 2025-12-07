// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_NETWORK_ACCESS_LOCAL_NETWORK_ACCESS_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_LOCAL_NETWORK_ACCESS_LOCAL_NETWORK_ACCESS_BROWSERTEST_BASE_H_

#include "base/command_line.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace local_network_access {

class LocalNetworkAccessBrowserTestBase : public policy::PolicyTest {
 public:
  using WebFeature = blink::mojom::WebFeature;
  // By default, maps all hosts to 127.0.0.1. Set to false if the tests need
  // control over which hosts resolve to localhost.
  explicit LocalNetworkAccessBrowserTestBase(
      bool map_all_hosts_to_localhost = true);
  ~LocalNetworkAccessBrowserTestBase() override;

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  // Fetch the Blink.UseCounter.Features histogram in every renderer process
  // until reaching, but not exceeding, |expected_count|.
  void CheckCounter(WebFeature feature, int expected_count);

  // Fetch the |histogram|'s |bucket| in every renderer process until reaching,
  // but not exceeding, |expected_count|.
  template <typename T>
  void CheckHistogramCount(std::string_view histogram,
                           T bucket,
                           int expected_count);
  permissions::PermissionRequestManager* GetPermissionRequestManager();
  permissions::MockPermissionPromptFactory* bubble_factory();

 protected:
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  const bool map_all_hosts_to_localhost_;
  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList features_;
  base::HistogramTester histogram_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
};

}  // namespace local_network_access

#endif  // CHROME_BROWSER_LOCAL_NETWORK_ACCESS_LOCAL_NETWORK_ACCESS_BROWSERTEST_BASE_H_
