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
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
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
    if (!js_result.value.is_bool()) {
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
    if (!js_result.value.is_bool()) {
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
  if (!js_result.value.is_string()) {
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

#if BUILDFLAG(IS_CHROMEOS)
class MockMultiCaptureService : public crosapi::mojom::MultiCaptureService {
 public:
  MockMultiCaptureService() = default;
  MockMultiCaptureService(const MockMultiCaptureService&) = delete;
  MockMultiCaptureService& operator=(const MockMultiCaptureService&) = delete;
  ~MockMultiCaptureService() override = default;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::MultiCaptureService> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // crosapi::mojom::MultiCaptureService:
  MOCK_METHOD(void,
              MultiCaptureStarted,
              (const std::string& label, const std::string& host),
              (override));
  MOCK_METHOD(void,
              MultiCaptureStopped,
              (const std::string& label),
              (override));
  MOCK_METHOD(void,
              MultiCaptureStartedFromApp,
              (const std::string& label,
               const std::string& app_id,
               const std::string& app_name),
              (override));
  MOCK_METHOD(void,
              IsMultiCaptureAllowed,
              (const GURL& url, IsMultiCaptureAllowedCallback),
              (override));
  MOCK_METHOD(void,
              IsMultiCaptureAllowedForAnyOriginOnMainProfile,
              (IsMultiCaptureAllowedForAnyOriginOnMainProfileCallback),
              (override));

 private:
  mojo::ReceiverSet<crosapi::mojom::MultiCaptureService> receivers_;
};
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  base::test::ScopedFeatureList scoped_feature_list_;
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
    capture_policy::SetMultiCaptureServiceForTesting(
        &mock_multi_capture_service_);

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
  testing::NiceMock<MockMultiCaptureService> mock_multi_capture_service_;
  const std::string method1_;
  const std::string method2_;
  raw_ptr<content::WebContents> contents_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    ProgrammaticallyStoppingOneDoesNotStopTheOther) {
  SetScreens(/*screen_count=*/1u);
  EXPECT_CALL(mock_multi_capture_service_,
              IsMultiCaptureAllowed(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  ASSERT_EQ(Run(method1_), nullptr);
  ASSERT_EQ(Run(method2_), nullptr);
  ASSERT_EQ(ProgrammaticallyStop(method1_), nullptr);

  EXPECT_FALSE(AreAllTracksLive(method1_).value.GetBool());
  EXPECT_TRUE(AreAllTracksLive(method2_).value.GetBool());
}

// Identical to StoppingOneDoesNotStopTheOther other than that this following
// test stops the second-started method first.
IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    ProgrammaticallyStoppingOneDoesNotStopTheOtherInverseOrder) {
  SetScreens(/*screen_count=*/1u);
  EXPECT_CALL(mock_multi_capture_service_,
              IsMultiCaptureAllowed(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  ASSERT_EQ(Run(method1_), nullptr);
  ASSERT_EQ(Run(method2_), nullptr);
  ASSERT_EQ(ProgrammaticallyStop(method2_), nullptr);

  EXPECT_TRUE(AreAllTracksLive(method1_).value.GetBool());
  EXPECT_FALSE(AreAllTracksLive(method2_).value.GetBool());
}

IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    UserStoppingGetDisplayMediaDoesNotStopGetAllScreensMedia) {
  SetScreens(/*screen_count=*/1u);
  EXPECT_CALL(mock_multi_capture_service_,
              IsMultiCaptureAllowed(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  ASSERT_EQ(Run(method1_), nullptr);
  ASSERT_EQ(Run(method2_), nullptr);

  // The capture which was started via getDisplayMedia() caused the
  // browser to show the user UX for stopping that capture. Simlate a user
  // interaction with that UX.
  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->StopMediaCapturing(
          contents_, MediaStreamCaptureIndicator::MediaType::kDisplayMedia);
  EXPECT_EQ(content::EvalJs(contents_,
                            "waitUntilStoppedByUser(\"getDisplayMedia\");"),
            nullptr);

  // Test-focus - the capture started through gASM was not affected
  // by the user's interaction with the capture started via gDM.
  EXPECT_TRUE(AreAllTracksLive("getAllScreensMedia").value.GetBool());
}

class MultiCaptureNotificationTest : public GetAllScreensMediaBrowserTestBase {
 public:
  MultiCaptureNotificationTest()
      : GetAllScreensMediaBrowserTestBase(/*is_permissions_policy_set=*/true) {}
  MultiCaptureNotificationTest(const MultiCaptureNotificationTest&) = delete;
  MultiCaptureNotificationTest& operator=(const MultiCaptureNotificationTest&) =
      delete;

  void SetUpOnMainThread() override {
    GetAllScreensMediaBrowserTestBase::SetUpOnMainThread();
    client_ = static_cast<ChromeContentBrowserClient*>(
        content::SetBrowserClientForTesting(nullptr));
    content::SetBrowserClientForTesting(client_);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    client_ = nullptr;
  }

 protected:
  ChromeContentBrowserClient* client() { return client_; }

  auto GetAllNotifications() {
    base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
    NotificationDisplayServiceFactory::GetForProfile(browser()->profile())
        ->GetDisplayed(get_displayed_future.GetCallback());
    const auto& notification_ids = get_displayed_future.Get<0>();
    EXPECT_TRUE(get_displayed_future.Wait());
    return notification_ids;
  }

  void ClearAllNotifications() {
    NotificationDisplayService* service =
        NotificationDisplayServiceFactory::GetForProfile(browser()->profile());
    for (const std::string& notification_id : GetAllNotifications()) {
      service->Close(NotificationHandler::Type::TRANSIENT, notification_id);
    }
  }

  size_t GetDisplayedNotificationsCount() {
    return GetAllNotifications().size();
  }

  void WaitUntilDisplayNotificationCount(size_t display_count) {
    ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
      return GetDisplayedNotificationsCount() == display_count;
    }));
  }

  void PostNotifyStateChanged(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      const std::string& label,
      content::ContentBrowserClient::MultiCaptureChanged state) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &content::ContentBrowserClient::NotifyMultiCaptureStateChanged,
            base::Unretained(client_), render_frame_host_id, label, state));
  }

  raw_ptr<ChromeContentBrowserClient> client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(MultiCaptureNotificationTest,
                       SingleRequestNotificationIsShown) {
  content::RenderFrameHost* app_frame = OpenApp(allowed_url_info_1().app_id());
  const auto renderer_id = app_frame->GetGlobalId();

  PostNotifyStateChanged(
      renderer_id,
      /*label=*/"testinglabel1",
      content::ContentBrowserClient::MultiCaptureChanged::kStarted);

  WaitUntilDisplayNotificationCount(/*display_count=*/2u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr("testinglabel1")),
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr("on_login"))));

  PostNotifyStateChanged(
      renderer_id,
      /*label=*/"testinglabel1",
      content::ContentBrowserClient::MultiCaptureChanged::kStopped);
  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
}

IN_PROC_BROWSER_TEST_F(MultiCaptureNotificationTest,
                       CalledFromAppSingleRequestNotificationIsShown) {
  content::RenderFrameHost* app_frame = OpenApp(allowed_url_info_1().app_id());
  const auto renderer_id = app_frame->GetGlobalId();

  PostNotifyStateChanged(
      renderer_id,
      /*label=*/"testinglabel",
      content::ContentBrowserClient::MultiCaptureChanged::kStarted);

  WaitUntilDisplayNotificationCount(/*display_count=*/2u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr("testinglabel")),
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr("on_login"))));

  PostNotifyStateChanged(
      renderer_id,
      /*label=*/"testinglabel",
      content::ContentBrowserClient::MultiCaptureChanged::kStopped);
  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
}

IN_PROC_BROWSER_TEST_F(MultiCaptureNotificationTest,
                       CalledFromAppMultipleRequestsNotificationsAreShown) {
  content::RenderFrameHost* app_frame_1 =
      OpenApp(allowed_url_info_1().app_id());
  const auto renderer_id_1 = app_frame_1->GetGlobalId();

  content::RenderFrameHost* app_frame_2 =
      OpenApp(allowed_url_info_2().app_id());
  const auto renderer_id_2 = app_frame_2->GetGlobalId();

  std::string expected_notifier_id_1 = "testinglabel1";
  PostNotifyStateChanged(
      renderer_id_1,
      /*label=*/expected_notifier_id_1,
      content::ContentBrowserClient::MultiCaptureChanged::kStarted);
  WaitUntilDisplayNotificationCount(/*display_count=*/2u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id_1)),
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr("on_login"))));

  std::string expected_notifier_id_2 = "testinglabel2";
  PostNotifyStateChanged(
      renderer_id_2,
      /*label=*/expected_notifier_id_2,
      content::ContentBrowserClient::MultiCaptureChanged::kStarted);
  WaitUntilDisplayNotificationCount(/*display_count=*/3u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id_1)),
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id_2)),
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr("on_login"))));

  PostNotifyStateChanged(
      renderer_id_2,
      /*label=*/expected_notifier_id_2,
      content::ContentBrowserClient::MultiCaptureChanged::kStopped);
  WaitUntilDisplayNotificationCount(/*display_count=*/2u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id_1)),
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr("on_login"))));

  PostNotifyStateChanged(
      renderer_id_1,
      /*label=*/expected_notifier_id_1,
      content::ContentBrowserClient::MultiCaptureChanged::kStopped);
  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
}
