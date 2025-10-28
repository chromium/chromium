// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class OneTimePermissionsBrowserTest : public InProcessBrowserTest {
 public:
  OneTimePermissionsBrowserTest() = default;
  ~OneTimePermissionsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
    manager_ = permissions::PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(manager_);
  }

  void TearDownOnMainThread() override {
    manager_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // This is needed to auto-accept the media stream permission requests.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitch("use-fake-ui-for-media-stream");
  }

  void RequestPermission(permissions::RequestType request_type) {
    permissions::PermissionRequestObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    test_api_->AddSimpleRequest(browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetPrimaryMainFrame(),
                                request_type);
    observer.Wait();
    manager_->AcceptThisTime();
  }

  GURL GetTestURL() {
    return https_server_->GetURL("a.test", "/permissions/requests.html");
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
  raw_ptr<permissions::PermissionRequestManager> manager_;
};

IN_PROC_BROWSER_TEST_F(OneTimePermissionsBrowserTest, RecordOneTimeGrant) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  base::HistogramTester histogram_tester;

  // Geolocation
  RequestPermission(permissions::RequestType::kGeolocation);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 1, 1);
  RequestPermission(permissions::RequestType::kGeolocation);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 2, 1);

  // Mic
  RequestPermission(permissions::RequestType::kMicStream);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.AudioCapture.OneTimeGrant", 1, 1);
  RequestPermission(permissions::RequestType::kMicStream);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.AudioCapture.OneTimeGrant", 2, 1);

  // Camera
  RequestPermission(permissions::RequestType::kCameraStream);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.VideoCapture.OneTimeGrant", 1, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionsBrowserTest, RecordGrantOTPCount) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  base::HistogramTester histogram_tester;
  auto* manager = permissions::PermissionRequestManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  test::PermissionRequestManagerTestApi test_api(manager);

  // Request Geolocation twice with "Allow this time"
  RequestPermission(permissions::RequestType::kGeolocation);
  RequestPermission(permissions::RequestType::kGeolocation);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 2, 1);

  // Now grant permanently
  permissions::PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  test_api.AddSimpleRequest(browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetPrimaryMainFrame(),
                            permissions::RequestType::kGeolocation);
  observer.Wait();
  manager->Accept();  // Permanent grant

  // Expect GrantOTPCount to be 2 for Geolocation
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.GrantOTPCount", 2, 1);

  // Request Mic once with "Allow this time"
  RequestPermission(permissions::RequestType::kMicStream);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.AudioCapture.OneTimeGrant", 1, 1);

  // Now grant permanently
  permissions::PermissionRequestObserver observer2(
      browser()->tab_strip_model()->GetActiveWebContents());
  test_api.AddSimpleRequest(browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetPrimaryMainFrame(),
                            permissions::RequestType::kMicStream);
  observer2.Wait();
  manager->Accept();  // Permanent grant

  // Expect GrantOTPCount to be 1 for AudioCapture
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.AudioCapture.GrantOTPCount", 1, 1);

  // Grant Camera permanently without any prior one-time grants
  permissions::PermissionRequestObserver observer3(
      browser()->tab_strip_model()->GetActiveWebContents());
  test_api.AddSimpleRequest(browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetPrimaryMainFrame(),
                            permissions::RequestType::kCameraStream);
  observer3.Wait();
  manager->Accept();  // Permanent grant

  // Expect GrantOTPCount to be 0 for VideoCapture
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.VideoCapture.GrantOTPCount", 0, 1);
}
