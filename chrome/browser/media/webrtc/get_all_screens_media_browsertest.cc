// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/test/display_manager_test_api.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/shell.h"
#include "base/path_service.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

std::string ExtractError(const std::string& message) {
  const std::vector<std::string> split_components = base::SplitString(
      message, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return (split_components.size() == 2 &&
          split_components[0] == "capture-failure")
             ? split_components[1]
             : "";
}

std::optional<base::FilePath> GetSourceDir() {
  base::FilePath source_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir)) {
    return std::nullopt;
  }
  return source_dir;
}

bool RunGetAllScreensMediaAndGetIds(content::WebContents* tab,
                                    std::set<std::string>& stream_ids,
                                    std::set<std::string>& track_ids,
                                    std::string* error_name_out = nullptr) {
  EXPECT_TRUE(stream_ids.empty());
  EXPECT_TRUE(track_ids.empty());

  {
    const content::EvalJsResult js_result = content::EvalJs(
        tab->GetPrimaryMainFrame(),
        "typeof navigator.mediaDevices.getAllScreensMedia === 'function';");
    if (!js_result.is_bool()) {
      ADD_FAILURE() << "Could not check existence of getAllScreensMedia.";
      return false;
    }

    if (!js_result.ExtractBool()) {
      *error_name_out = "FunctionNotFoundError";
      return false;
    }
  }

  {
    const content::EvalJsResult js_result = content::EvalJs(
        tab->GetPrimaryMainFrame(),
        "typeof runGetAllScreensMediaAndGetIds === 'function';");
    if (!js_result.is_bool()) {
      ADD_FAILURE()
          << "Could not check existence of runGetAllScreensMediaAndGetIds.";
      return false;
    }

    if (!js_result.ExtractBool()) {
      *error_name_out = "ScriptNotLoadedError";
      return false;
    }
  }

  const content::EvalJsResult js_result = content::EvalJs(
      tab->GetPrimaryMainFrame(), "runGetAllScreensMediaAndGetIds();");
  if (!js_result.is_string()) {
    ADD_FAILURE() << "Could not run runGetAllScreensMediaAndGetIds.";
    return false;
  }

  std::string result = js_result.ExtractString();
  const std::vector<std::string> split_id_components = base::SplitString(
      result, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split_id_components.size() != 2) {
    if (error_name_out) {
      *error_name_out = ExtractError(result);
    }
    return false;
  }

  std::vector<std::string> stream_ids_vector = base::SplitString(
      split_id_components[0], ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  stream_ids =
      std::set<std::string>(stream_ids_vector.begin(), stream_ids_vector.end());

  std::vector<std::string> track_ids_vector = base::SplitString(
      split_id_components[1], ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  track_ids =
      std::set<std::string>(track_ids_vector.begin(), track_ids_vector.end());

  return true;
}

void SetScreens(size_t screen_count) {
#if BUILDFLAG(IS_CHROMEOS)
  // This part of the test only works on ChromeOS.
  std::stringstream screens;
  for (size_t screen_index = 0; screen_index + 1 < screen_count;
       screen_index++) {
    // Each entry in this comma separated list corresponds to a screen
    // specification following the format defined in
    // |ManagedDisplayInfo::CreateFromSpec|.
    // The used specification simulates screens with resolution 640x480
    // at the host coordinates (screen_index * 640, 0).
    screens << screen_index * 640 << "+0-640x480,";
  }
  if (screen_count != 0) {
    screens << (screen_count - 1) * 640 << "+0-640x480";
  }
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay(screens.str());
#endif
}

}  // namespace

class GetAllScreensMediaBrowserTestBase
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  explicit GetAllScreensMediaBrowserTestBase(bool is_permissions_policy_set)
      : is_permissions_policy_set_(is_permissions_policy_set) {
    allowed_app_1_ =
        CreateIsolatedWebApp(/*html_text=*/"GetAllScreensMedia allowed 1");
    EXPECT_TRUE(allowed_app_1_);

    allowed_app_2_ =
        CreateIsolatedWebApp(/*html_text=*/"GetAllScreensMedia allowed 2");
    EXPECT_TRUE(allowed_app_2_);

    denied_app_ =
        CreateIsolatedWebApp(/*html_text=*/"GetAllScreensMedia denied");
    EXPECT_TRUE(denied_app_);
  }

  GetAllScreensMediaBrowserTestBase(const GetAllScreensMediaBrowserTestBase&) =
      delete;
  GetAllScreensMediaBrowserTestBase& operator=(
      const GetAllScreensMediaBrowserTestBase&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    web_app::IsolatedWebAppBrowserTestHarness::
        SetUpInProcessBrowserTestFixture();

    // For Ash, and end2end test is possible because the policy value can be
    // set up before the keyed service can cache the value.
#if BUILDFLAG(IS_CHROMEOS)
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    SetAllowedOriginsPolicy(/*allow_listed_origins=*/{
        "isolated-app://" + allowed_app_1_->web_bundle_id().id(),
        "isolated-app://" + allowed_app_2_->web_bundle_id().id()});
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void SetUpOnMainThread() override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    allowed_app_1_url_info_ = allowed_app_1_->Install(profile()).value();
    allowed_app_2_url_info_ = allowed_app_2_->Install(profile()).value();
    denied_app_url_info_ = denied_app_->Install(profile()).value();
  }

  void TearDownOnMainThread() override {
    CloseAllApps();
    web_app::IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> CreateIsolatedWebApp(
      const std::string html_text) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto manifest_builder =
        web_app::ManifestBuilder().SetName("app-3.0.4").SetVersion("3.0.4");
    if (IsPermissionPolicySet()) {
      manifest_builder.AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kAllScreensCapture,
          /*self=*/true, /*origins=*/{});
      manifest_builder.AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kDisplayCapture,
          /*self=*/true, /*origins=*/{});
    }
    auto builder = web_app::IsolatedWebAppBuilder(std::move(manifest_builder));

    std::optional<base::FilePath> source_dir = GetSourceDir();
    if (!source_dir) {
      ADD_FAILURE() << "Could not determine source directory.";
      return nullptr;
    }

    builder.AddHtml("/", R"(
            <head>
            <script type="text/javascript" src="/test_functions.js">
            </script>
            <script type="text/javascript"
              src="/get_all_screens_media_functions.js">
            </script>
            <title>3.0.4</title>
          </head>
          <body>
            <h1>)" + html_text +
                             "</h1></body>)");
    builder.AddFileFromDisk("test_functions.js",
                            source_dir->Append(FILE_PATH_LITERAL(
                                "chrome/test/data/webrtc/test_functions.js")));

    builder.AddFileFromDisk(
        "get_all_screens_media_functions.js",
        source_dir->Append(FILE_PATH_LITERAL(
            "chrome/test/data/webrtc/get_all_screens_media_functions.js")));
    auto app = builder.BuildBundle();
    app->TrustSigningKey();
    return app;
  }

  void CloseAllApps() {
    CloseApp(allowed_url_info_1());
    CloseApp(allowed_url_info_2());
    CloseApp(denied_url_info());
  }

  void CloseApp(const web_app::IsolatedWebAppUrlInfo& url_info) {
    base::test::TestFuture<void> app_closed_future;
    provider().ui_manager().NotifyOnAllAppWindowsClosed(
        url_info.app_id(), app_closed_future.GetCallback());
    provider().ui_manager().CloseAppWindows(url_info.app_id());
    EXPECT_TRUE(app_closed_future.Wait());
  }

  bool IsPermissionPolicySet() const { return is_permissions_policy_set_; }

  void SetAllowedOriginsPolicy(
      const std::vector<std::string>& allow_listed_origins) {
    policy::PolicyMap policies;
    base::Value::List allowed_origins;
    for (const auto& allowed_origin : allow_listed_origins) {
      allowed_origins.Append(base::Value(allowed_origin));
    }
    policies.Set(policy::key::kMultiScreenCaptureAllowedForUrls,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(allowed_origins)), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  const web_app::IsolatedWebAppUrlInfo& allowed_url_info_1() const {
    CHECK(allowed_app_1_url_info_);
    return *allowed_app_1_url_info_;
  }

  const web_app::IsolatedWebAppUrlInfo& allowed_url_info_2() const {
    CHECK(allowed_app_2_url_info_);
    return *allowed_app_2_url_info_;
  }

  const web_app::IsolatedWebAppUrlInfo& denied_url_info() const {
    CHECK(denied_app_url_info_);
    return *denied_app_url_info_;
  }

 protected:
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> allowed_app_1_;
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> allowed_app_2_;
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> denied_app_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  std::optional<web_app::IsolatedWebAppUrlInfo> allowed_app_1_url_info_;
  std::optional<web_app::IsolatedWebAppUrlInfo> allowed_app_2_url_info_;
  std::optional<web_app::IsolatedWebAppUrlInfo> denied_app_url_info_;

 private:
  const bool is_permissions_policy_set_;
};

class MultiScreenCaptureInIsolatedWebAppBrowserTest
    : public GetAllScreensMediaBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  MultiScreenCaptureInIsolatedWebAppBrowserTest()
      : GetAllScreensMediaBrowserTestBase(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         MultiScreenCaptureInIsolatedWebAppBrowserTest,
                         // Determines whether the `all-screens-capture`
                         // permission policy is defined in the manifest.
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(MultiScreenCaptureInIsolatedWebAppBrowserTest,
                       GetAllScreensMediaSuccessful) {
  SetScreens(/*screen_count=*/1u);
  content::RenderFrameHost* app_frame = OpenApp(allowed_url_info_1().app_id());

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(
      content::WebContents::FromRenderFrameHost(app_frame), stream_ids,
      track_ids, &error_name);

  if (IsPermissionPolicySet()) {
    EXPECT_TRUE(result);
    EXPECT_EQ(track_ids.size(), 1u);
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ(track_ids.size(), 0u);
  }
}

IN_PROC_BROWSER_TEST_P(MultiScreenCaptureInIsolatedWebAppBrowserTest,
                       GetAllScreensMediaDenied) {
  SetScreens(/*screen_count=*/1u);

  content::RenderFrameHost* app_frame = OpenApp(denied_url_info().app_id());

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(
      content::WebContents::FromRenderFrameHost(app_frame), stream_ids,
      track_ids, &error_name);
  EXPECT_FALSE(result);
}

// Test that getDisplayMedia and getAllScreensMedia are independent,
// so stopping one will not stop the other.
class InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest
    : public GetAllScreensMediaBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest()
      : GetAllScreensMediaBrowserTestBase(/*is_permissions_policy_set=*/true),
        method1_(GetParam() ? "getDisplayMedia" : "getAllScreensMedia"),
        method2_(GetParam() ? "getAllScreensMedia" : "getDisplayMedia") {}

  void SetUpOnMainThread() override {
    GetAllScreensMediaBrowserTestBase::SetUpOnMainThread();
    contents_ = content::WebContents::FromRenderFrameHost(
        OpenApp(allowed_url_info_1().app_id()));
  }

  void TearDownOnMainThread() override {
    contents_ = nullptr;
    GetAllScreensMediaBrowserTestBase::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Flags use to automatically select the right desktop source and get
    // around security restrictions.
    // TODO(crbug.com/40274188): Use a less error-prone flag.
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Display");
#else
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Entire screen");
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  content::EvalJsResult Run(const std::string& method) {
    return content::EvalJs(contents_,
                           base::StringPrintf("run(\"%s\");", method.c_str()));
  }

  content::EvalJsResult ProgrammaticallyStop(const std::string& method) {
    return content::EvalJs(contents_,
                           base::StringPrintf("stop(\"%s\");", method.c_str()));
  }

  content::EvalJsResult AreAllTracksLive(const std::string& method) {
    return content::EvalJs(
        contents_,
        base::StringPrintf("areAllTracksLive(\"%s\");", method.c_str()));
  }

 protected:
  const std::string method1_;
  const std::string method2_;
  raw_ptr<content::WebContents> contents_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    ::testing::Bool());

// crbug.com/441674610: Disabled as failing on ChromeOS without being able to
// reproduce.
IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    DISABLED_ProgrammaticallyStoppingOneDoesNotStopTheOther) {
  SetScreens(/*screen_count=*/1u);
  ASSERT_EQ(Run(method1_), base::Value());
  ASSERT_EQ(Run(method2_), base::Value());
  ASSERT_EQ(ProgrammaticallyStop(method1_), base::Value());

  EXPECT_EQ(false, AreAllTracksLive(method1_));
  EXPECT_EQ(true, AreAllTracksLive(method2_));
}

// crbug.com/441674610: Disabled as failing on ChromeOS without being able to
// reproduce. Identical to StoppingOneDoesNotStopTheOther other than that this
// following test stops the second-started method first.
IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    DISABLED_ProgrammaticallyStoppingOneDoesNotStopTheOtherInverseOrder) {
  SetScreens(/*screen_count=*/1u);
  ASSERT_EQ(Run(method1_), base::Value());
  ASSERT_EQ(Run(method2_), base::Value());
  ASSERT_EQ(ProgrammaticallyStop(method2_), base::Value());

  EXPECT_EQ(true, AreAllTracksLive(method1_));
  EXPECT_EQ(false, AreAllTracksLive(method2_));
}

// crbug.com/441674610: Disabled as failing on ChromeOS without being able to
// reproduce.
IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    DISABLED_UserStoppingGetDisplayMediaDoesNotStopGetAllScreensMedia) {
  SetScreens(/*screen_count=*/1u);
  ASSERT_EQ(Run(method1_), base::Value());
  ASSERT_EQ(Run(method2_), base::Value());

  // The capture which was started via getDisplayMedia() caused the
  // browser to show the user UX for stopping that capture. Simlate a user
  // interaction with that UX.
  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->StopMediaCapturing(
          contents_, MediaStreamCaptureIndicator::MediaType::kDisplayMedia);
  EXPECT_EQ(content::EvalJs(contents_,
                            "waitUntilStoppedByUser(\"getDisplayMedia\");"),
            base::Value());

  // Test-focus - the capture started through gASM was not affected
  // by the user's interaction with the capture started via gDM.
  EXPECT_EQ(true, AreAllTracksLive("getAllScreensMedia"));
}
