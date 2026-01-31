// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class OneTimePermissionsBrowserTestBase : public InProcessBrowserTest {
 public:
  OneTimePermissionsBrowserTestBase() = default;
  ~OneTimePermissionsBrowserTestBase() override = default;

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

  void RequestPermissionAnAcceptThisTime(
      permissions::RequestType request_type) {
    permissions::PermissionRequestObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    test_api_->AddSimpleRequest(browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetPrimaryMainFrame(),
                                request_type);
    observer.Wait();
    PromptOptions prompt_options =
        (request_type == permissions::RequestType::kGeolocation &&
         base::FeatureList::IsEnabled(
             content_settings::features::kApproximateGeolocationPermission))
            ? PromptOptions(GeolocationPromptOptions{
                  .selected_accuracy = GeolocationAccuracy::kPrecise})
            : std::monostate();
    manager_->AcceptThisTime(prompt_options);
  }

  GURL GetTestURL() {
    return https_server_->GetURL("a.test", "/permissions/requests.html");
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
  raw_ptr<permissions::PermissionRequestManager> manager_;
};

class OneTimePermissionsBrowserTest : public base::test::WithFeatureOverride,
                                      public OneTimePermissionsBrowserTestBase {
 public:
  OneTimePermissionsBrowserTest()
      : base::test::WithFeatureOverride(
            content_settings::features::kApproximateGeolocationPermission) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(OneTimePermissionsBrowserTest);

IN_PROC_BROWSER_TEST_P(OneTimePermissionsBrowserTest, RecordOneTimeGrant) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  base::HistogramTester histogram_tester;

  // Geolocation
  RequestPermissionAnAcceptThisTime(permissions::RequestType::kGeolocation);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 1, 1);
  RequestPermissionAnAcceptThisTime(permissions::RequestType::kGeolocation);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 2, 1);

  // Mic
  RequestPermissionAnAcceptThisTime(permissions::RequestType::kMicStream);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.AudioCapture.OneTimeGrant", 1, 1);
  RequestPermissionAnAcceptThisTime(permissions::RequestType::kMicStream);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.AudioCapture.OneTimeGrant", 2, 1);

  // Camera
  RequestPermissionAnAcceptThisTime(permissions::RequestType::kCameraStream);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.VideoCapture.OneTimeGrant", 1, 1);
}

struct OneTimePermissionActionTestParams {
  permissions::PermissionAction action;
  std::string histogram_suffix;
  permissions::RequestType request_type;
  std::string permission_type_string;
  bool approximate_geolocation_enabled = false;
};

class OneTimePermissionActionBrowserTest
    : public OneTimePermissionsBrowserTestBase,
      public testing::WithParamInterface<OneTimePermissionActionTestParams> {
 public:
  OneTimePermissionActionBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        content_settings::features::kApproximateGeolocationPermission,
        GetParam().approximate_geolocation_enabled);
  }
  std::string GetOneTimePermissionActionHistogramName() {
    return "Permissions.OneTimePermission." +
           GetParam().permission_type_string + "." +
           GetParam().histogram_suffix;
  }

  std::string GetOneTimeGrantHistogramName() {
    return "Permissions.OneTimePermission." +
           GetParam().permission_type_string + ".OneTimeGrant";
  }

  void ExecuteAction(permissions::PermissionRequestManager* manager) {
    PromptOptions prompt_options =
        (GetParam().request_type == permissions::RequestType::kGeolocation &&
         base::FeatureList::IsEnabled(
             content_settings::features::kApproximateGeolocationPermission))
            ? PromptOptions(GeolocationPromptOptions{
                  .selected_accuracy = GeolocationAccuracy::kPrecise})
            : std::monostate();
    switch (GetParam().action) {
      case permissions::PermissionAction::GRANTED:
        manager->Accept(prompt_options);
        break;
      case permissions::PermissionAction::DENIED:
        manager->Deny(prompt_options);
        break;
      case permissions::PermissionAction::DISMISSED:
        manager->Dismiss(prompt_options);
        break;
      case permissions::PermissionAction::IGNORED:
        manager->Ignore(prompt_options);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(OneTimePermissionActionBrowserTest, RecordOTPCount) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  base::HistogramTester histogram_tester;
  auto* manager = permissions::PermissionRequestManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  test::PermissionRequestManagerTestApi test_api(manager);

  RequestPermissionAnAcceptThisTime(GetParam().request_type);
  RequestPermissionAnAcceptThisTime(GetParam().request_type);
  // I.e. Permissions.OneTimePermission.Geolocation.OneTimeGrant
  histogram_tester.ExpectBucketCount(GetOneTimeGrantHistogramName(), 1, 1);
  histogram_tester.ExpectBucketCount(GetOneTimeGrantHistogramName(), 2, 1);

  // Trigger the permanent action (Grant/Deny/Dismiss/Ignore)
  permissions::PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  test_api.AddSimpleRequest(browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetPrimaryMainFrame(),
                            GetParam().request_type);
  observer.Wait();

  ExecuteAction(manager);

  // Expect correct histogram count, i.e. for
  // Permissions.OneTimePermission.Geolocation.GrantOTPCount
  histogram_tester.ExpectBucketCount(GetOneTimePermissionActionHistogramName(),
                                     2, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OneTimePermissionActionBrowserTest,
    testing::Values(
        // Geolocation
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::GRANTED, "GrantOTPCount",
            permissions::RequestType::kGeolocation, "Geolocation"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::DENIED, "DenyOTPCount",
            permissions::RequestType::kGeolocation, "Geolocation"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::DISMISSED, "DismissOTPCount",
            permissions::RequestType::kGeolocation, "Geolocation"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::IGNORED, "IgnoreOTPCount",
            permissions::RequestType::kGeolocation, "Geolocation"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::GRANTED, "GrantOTPCount",
            permissions::RequestType::kGeolocation, "Geolocation", true},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::DENIED, "DenyOTPCount",
            permissions::RequestType::kGeolocation, "Geolocation", true},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::DISMISSED, "DismissOTPCount",
            permissions::RequestType::kGeolocation, "Geolocation", true},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::IGNORED, "IgnoreOTPCount",
            permissions::RequestType::kGeolocation, "Geolocation", true},
        // Mic
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::GRANTED, "GrantOTPCount",
            permissions::RequestType::kMicStream, "AudioCapture"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::DENIED, "DenyOTPCount",
            permissions::RequestType::kMicStream, "AudioCapture"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::DISMISSED, "DismissOTPCount",
            permissions::RequestType::kMicStream, "AudioCapture"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::IGNORED, "IgnoreOTPCount",
            permissions::RequestType::kMicStream, "AudioCapture"},
        // Camera
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::GRANTED, "GrantOTPCount",
            permissions::RequestType::kCameraStream, "VideoCapture"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::DENIED, "DenyOTPCount",
            permissions::RequestType::kCameraStream, "VideoCapture"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::DISMISSED, "DismissOTPCount",
            permissions::RequestType::kCameraStream, "VideoCapture"},
        OneTimePermissionActionTestParams{
            permissions::PermissionAction::IGNORED, "IgnoreOTPCount",
            permissions::RequestType::kCameraStream, "VideoCapture"}),
    [](const testing::TestParamInfo<OneTimePermissionActionTestParams>& info) {
      std::string action_str;
      switch (info.param.action) {
        case permissions::PermissionAction::GRANTED:
          action_str = "Granted";
          break;
        case permissions::PermissionAction::DENIED:
          action_str = "Denied";
          break;
        case permissions::PermissionAction::DISMISSED:
          action_str = "Dismissed";
          break;
        case permissions::PermissionAction::IGNORED:
          action_str = "Ignored";
          break;
        default:
          action_str = "Unknown";
      }
      return info.param.permission_type_string + "_" + action_str +
             (info.param.approximate_geolocation_enabled
                  ? "_withApproximateLocation"
                  : "");
    });
