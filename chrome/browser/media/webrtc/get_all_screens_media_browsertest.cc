// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/gtest_tags.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/test/display_manager_test_api.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/path_service.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "chrome/browser/notifications/notification_display_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/message_center.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

struct GetAllScreensMediaTestParameters {
  std::string base_page;
  bool expected_csp_acceptable;
  std::string expected_error_name;
  bool expected_script_should_load;
};

std::string ExtractError(const std::string& message) {
  const std::vector<std::string> split_components = base::SplitString(
      message, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return (split_components.size() == 2 &&
          split_components[0] == "capture-failure")
             ? split_components[1]
             : "";
}

bool RunGetAllScreensMediaAndGetIds(content::WebContents* tab,
                                    std::set<std::string>& stream_ids,
                                    std::set<std::string>& track_ids,
                                    std::string* error_name_out = nullptr) {
  EXPECT_TRUE(stream_ids.empty());
  EXPECT_TRUE(track_ids.empty());

  auto run_get_all_screens_media_exists =
      content::EvalJs(tab->GetPrimaryMainFrame(),
                      "typeof runGetAllScreensMediaAndGetIds === 'function';")
          .ExtractBool();
  if (!run_get_all_screens_media_exists) {
    *error_name_out = "ScriptNotLoadedError";
    return false;
  }

  std::string result = content::EvalJs(tab->GetPrimaryMainFrame(),
                                       "runGetAllScreensMediaAndGetIds();")
                           .ExtractString();

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

bool CheckScreenDetailedExists(content::WebContents* tab,
                               const std::string& track_id) {
  static constexpr char kVideoTrackContainsScreenDetailsCall[] =
      R"JS(videoTrackContainsScreenDetailed("%s"))JS";
  return content::EvalJs(
             tab->GetPrimaryMainFrame(),
             base::StringPrintf(kVideoTrackContainsScreenDetailsCall,
                                track_id.c_str()))
             .ExtractString() == "success-screen-detailed";
}

void SetScreens(size_t screen_count) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#else
  base::test::TestFuture<void> future;
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->UpdateDisplay((uint8_t)screen_count, future.GetCallback());
  ASSERT_TRUE(future.Wait());
#endif
}

bool SupportsDisplaySetting() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::TestController>() <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kUpdateDisplayMinVersion)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return true;
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

 private:
  mojo::ReceiverSet<crosapi::mojom::MultiCaptureService> receivers_;
};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class GetAllScreensMediaBrowserTestBase : public WebRtcTestBase {
 public:
  explicit GetAllScreensMediaBrowserTestBase(const std::string& base_page)
      : base_page_(base_page) {
    scoped_feature_list_.InitFromCommandLine(
        /*enable_features=*/
        "GetAllScreensMedia",
        /*disable_features=*/"");
  }

  void SetUpOnMainThread() override {
    if (!SupportsDisplaySetting()) {
      GTEST_SKIP();
    }

    WebRtcTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    contents_ = OpenTestPageInNewTab(base_page_);
    DCHECK(contents_);
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void TearDownOnMainThread() override {
    if (SupportsDisplaySetting()) {
      SetScreens(/*screen_count=*/1u);
    }
    WebRtcTestBase::TearDownOnMainThread();
  }
#endif  // BUIDLFALG(IS_CHROMEOS_LACROS)

 protected:
  raw_ptr<content::WebContents, DanglingUntriaged> contents_ = nullptr;

 private:
  std::string base_page_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows_;
};

class GetAllScreensMediaBrowserTest
    : public GetAllScreensMediaBrowserTestBase,
      public ::testing::WithParamInterface<GetAllScreensMediaTestParameters> {
 public:
  GetAllScreensMediaBrowserTest()
      : GetAllScreensMediaBrowserTestBase(GetParam().base_page) {}

  void SetUpOnMainThread() override {
    GetAllScreensMediaBrowserTestBase::SetUpOnMainThread();
    capture_policy::SetMultiCaptureServiceForTesting(
        &mock_multi_capture_service_);
  }

 protected:
  testing::StrictMock<MockMultiCaptureService> mock_multi_capture_service_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    GetAllScreensMediaBrowserTest,
    ::testing::ValuesIn(std::vector<GetAllScreensMediaTestParameters>{
        {/*base_page=*/"/webrtc/webrtc_getallscreensmedia_valid_csp_test.html",
         /*expected_csp_acceptable=*/true,
         /*expected_error_name=*/"",
         /*expected_script_should_load=*/true},
        {/*base_page=*/
         "/webrtc/webrtc_getallscreensmedia_valid_multiple_csps_test.html",
         /*expected_csp_acceptable=*/true,
         /*expected_error_name=*/"",
         /*expected_script_should_load=*/true},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_no_object_source_test.html",
         /*expected_csp_acceptable=*/false,
         /*expected_error_name=*/"NotAllowedError",
         /*expected_script_should_load=*/true},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_no_base_uri_test.html",
         /*expected_csp_acceptable=*/false,
         /*expected_error_name=*/"NotAllowedError",
         /*expected_script_should_load=*/true},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_no_script_source_test.html",
         /*expected_csp_acceptable=*/false,
         /*expected_error_name=*/"NotAllowedError",
         /*expected_script_should_load=*/true},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_no_trusted_types_test.html",
         /*expected_csp_acceptable=*/false,
         /*expected_error_name=*/"NotAllowedError",
         /*expected_script_should_load=*/true},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_invalid_csp_test.html",
         /*expected_csp_acceptable=*/false,
         /*expected_error_name=*/"ScriptNotLoadedError",
         /*expected_script_should_load=*/false},
    }));

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       GetAllScreensMediaSingleScreenAccessBasedOnCSP) {
  SetScreens(/*screen_count=*/1u);
  const auto& param = GetParam();
  if (param.expected_csp_acceptable) {
    EXPECT_CALL(mock_multi_capture_service_,
                IsMultiCaptureAllowed(testing::_, testing::_))
        .WillOnce(testing::Invoke(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(true);
            }));
  }

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(contents_, stream_ids,
                                                     track_ids, &error_name);
  if (param.expected_csp_acceptable) {
    EXPECT_TRUE(result);
    EXPECT_EQ(1u, track_ids.size());
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ(param.expected_error_name, error_name);
  }
}

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       GetAllScreensMediaNoScreenSuccessIfStrictCSP) {
  SetScreens(/*screen_count=*/1u);
  const auto& param = GetParam();
  if (param.expected_csp_acceptable) {
    EXPECT_CALL(mock_multi_capture_service_,
                IsMultiCaptureAllowed(testing::_, testing::_))
        .WillOnce(testing::Invoke(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(true);
            }));
  }

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(contents_, stream_ids,
                                                     track_ids, &error_name);
  if (param.expected_csp_acceptable) {
    EXPECT_TRUE(result);
    // If no screen is attached to a device, the |DisplayManager| will add a
    // default device. This same behavior is used in other places in Chrome that
    // handle multiple screens (e.g. in JS window.getScreenDetails() API) and
    // getAllScreensMedia will follow the same convention.
    EXPECT_EQ(1u, stream_ids.size());
    EXPECT_EQ(1u, track_ids.size());
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ(param.expected_error_name, error_name);
  }
}

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       GetAllScreensMediaMultipleScreensSuccessIfStrictCSP) {
  base::AddTagToTestResult("feature_id",
                           "screenplay-f3601ae4-bff7-495a-a51f-3c0997a46445");
  SetScreens(/*screen_count=*/5u);
  const auto& param = GetParam();
  if (param.expected_csp_acceptable) {
    EXPECT_CALL(mock_multi_capture_service_,
                IsMultiCaptureAllowed(testing::_, testing::_))
        .WillOnce(testing::Invoke(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(true);
            }));
  }

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(contents_, stream_ids,
                                                     track_ids, &error_name);
  if (param.expected_csp_acceptable) {
    EXPECT_TRUE(result);
    // TODO(crbug.com/1404274): Adapt this test if a decision is made on whether
    // stream ids shall be shared or unique.
    EXPECT_EQ(1u, stream_ids.size());
    EXPECT_EQ(5u, track_ids.size());
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ(param.expected_error_name, error_name);
  }
}

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       TrackContainsScreenDetailedIfStrictCSP) {
  SetScreens(/*screen_count=*/1u);
  const auto& param = GetParam();
  if (param.expected_csp_acceptable) {
    EXPECT_CALL(mock_multi_capture_service_,
                IsMultiCaptureAllowed(testing::_, testing::_))
        .WillOnce(testing::Invoke(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(true);
            }));
  }

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(contents_, stream_ids,
                                                     track_ids, &error_name);
  if (param.expected_csp_acceptable) {
    EXPECT_TRUE(result);
    EXPECT_TRUE(result);
    ASSERT_EQ(1u, stream_ids.size());
    ASSERT_EQ(1u, track_ids.size());

    EXPECT_TRUE(CheckScreenDetailedExists(contents_, *track_ids.begin()));
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ(param.expected_error_name, error_name);
  }
}

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       AutoSelectAllScreensNotAllowedByAdminPolicy) {
  SetScreens(/*screen_count=*/1u);
  const auto& param = GetParam();
  if (param.expected_csp_acceptable) {
    EXPECT_CALL(mock_multi_capture_service_,
                IsMultiCaptureAllowed(testing::_, testing::_))
        .WillOnce(testing::Invoke(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            }));
  }

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  EXPECT_FALSE(RunGetAllScreensMediaAndGetIds(contents_, stream_ids, track_ids,
                                              &error_name));
  EXPECT_EQ(param.expected_script_should_load ? "NotAllowedError"
                                              : "ScriptNotLoadedError",
            error_name);
}

// Test that getDisplayMedia and getAllScreensMedia are independent,
// so stopping one will not stop the other.
class InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest
    : public GetAllScreensMediaBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest()
      : GetAllScreensMediaBrowserTestBase(
            /*base_page=*/
            "/webrtc/webrtc_getallscreensmedia_valid_csp_test.html"),
        method1_(GetParam() ? "getDisplayMedia" : "getAllScreensMedia"),
        method2_(GetParam() ? "getAllScreensMedia" : "getDisplayMedia") {}

  void SetUpOnMainThread() override {
    GetAllScreensMediaBrowserTestBase::SetUpOnMainThread();
    capture_policy::SetMultiCaptureServiceForTesting(
        &mock_multi_capture_service_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Flags use to automatically select the right desktop source and get
    // around security restrictions.
    // TODO(crbug.com/1459164): Use a less error-prone flag.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Display");
#else
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Entire screen");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
  testing::StrictMock<MockMultiCaptureService> mock_multi_capture_service_;
  const std::string method1_;
  const std::string method2_;
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

  ASSERT_EQ(nullptr, Run(method1_));
  ASSERT_EQ(nullptr, Run(method2_));
  ASSERT_EQ(nullptr, ProgrammaticallyStop(method1_));

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

  ASSERT_EQ(nullptr, Run(method1_));
  ASSERT_EQ(nullptr, Run(method2_));
  ASSERT_EQ(nullptr, ProgrammaticallyStop(method2_));

  EXPECT_TRUE(AreAllTracksLive(method1_).value.GetBool());
  EXPECT_FALSE(AreAllTracksLive(method2_).value.GetBool());
}

// TODO(crbug.com/1479984): re-enable once the bug is fixed.
IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    DISABLED_UserStoppingGetDisplayMediaDoesNotStopGetAllScreensMedia) {
  SetScreens(/*screen_count=*/1u);
  EXPECT_CALL(mock_multi_capture_service_,
              IsMultiCaptureAllowed(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  ASSERT_EQ(nullptr, Run(method1_));
  ASSERT_EQ(nullptr, Run(method2_));

  // The capture which was started via getDisplayMedia() caused the
  // browser to show the user UX for stopping that capture. Simlate a user
  // interaction with that UX.
  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->StopMediaCapturing(
          contents_, MediaStreamCaptureIndicator::MediaType::kDisplayMedia);
  EXPECT_EQ(nullptr,
            content::EvalJs(contents_,
                            "waitUntilStoppedByUser(\"getDisplayMedia\");"));

  // Test-focus - the capture started through gASM was not affected
  // by the user's interaction with the capture started via gDM.
  EXPECT_TRUE(AreAllTracksLive("getAllScreensMedia").value.GetBool());
}

class MultiCaptureNotificationTest : public InProcessBrowserTest {
 public:
  MultiCaptureNotificationTest() = default;
  MultiCaptureNotificationTest(const MultiCaptureNotificationTest&) = delete;
  MultiCaptureNotificationTest& operator=(const MultiCaptureNotificationTest&) =
      delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    client_ = static_cast<ChromeContentBrowserClient*>(
        content::SetBrowserClientForTesting(nullptr));
    content::SetBrowserClientForTesting(client_);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    ClearAllNotifications();
    WaitUntilDisplayNotificationCount(/*display_count=*/0u);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    client_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    ClearAllNotifications();
    WaitUntilDisplayNotificationCount(/*display_count=*/0u);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

 protected:
  ChromeContentBrowserClient* client() { return client_; }

  auto GetAllNotifications() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
    NotificationDisplayService::GetForProfile(browser()->profile())
        ->GetDisplayed(get_displayed_future.GetCallback());
#else
    base::test::TestFuture<const std::vector<std::string>&>
        get_displayed_future;
    auto& remote = chromeos::LacrosService::Get()
                       ->GetRemote<crosapi::mojom::MessageCenter>();
    EXPECT_TRUE(remote.get());
    remote->GetDisplayedNotifications(get_displayed_future.GetCallback());
#endif
    const auto& notification_ids = get_displayed_future.Get<0>();
    EXPECT_TRUE(get_displayed_future.Wait());
    return notification_ids;
  }

  void ClearAllNotifications() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NotificationDisplayService* service =
        NotificationDisplayService::GetForProfile(browser()->profile());
#else
    base::test::TestFuture<const std::vector<std::string>&>
        get_displayed_future;
    auto& service = chromeos::LacrosService::Get()
                        ->GetRemote<crosapi::mojom::MessageCenter>();
    EXPECT_TRUE(service.get());
#endif
    for (const std::string& notification_id : GetAllNotifications()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      service->Close(NotificationHandler::Type::TRANSIENT, notification_id);
#else
      service->CloseNotification(notification_id);
#endif
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

  webapps::AppId InstallPWA(Profile* profile, const GURL& start_url) {
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    return web_app::test::InstallWebApp(profile, std::move(web_app_info));
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

  bool NotificationIdContainsLabel() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    return chromeos::LacrosService::Get()
               ->GetInterfaceVersion<crosapi::mojom::MultiCaptureService>() >=
           (int)crosapi::mojom::MultiCaptureService::MethodMinVersions::
               kMultiCaptureStartedFromAppMinVersion;
#else
    NOTREACHED();
#endif
  }

  raw_ptr<ChromeContentBrowserClient> client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(MultiCaptureNotificationTest,
                       SingleRequestNotificationIsShown) {
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const auto renderer_id = browser()
                               ->tab_strip_model()
                               ->GetWebContentsAt(0)
                               ->GetPrimaryMainFrame()
                               ->GetGlobalId();

  PostNotifyStateChanged(
      renderer_id,
      /*label=*/"testinglabel1",
      content::ContentBrowserClient::MultiCaptureChanged::kStarted);

  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(url.host()))));

  PostNotifyStateChanged(
      renderer_id,
      /*label=*/"testinglabel1",
      content::ContentBrowserClient::MultiCaptureChanged::kStopped);
  WaitUntilDisplayNotificationCount(/*display_count=*/0u);
}

IN_PROC_BROWSER_TEST_F(MultiCaptureNotificationTest,
                       CalledFromAppSingleRequestNotificationIsShown) {
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(
      browser()->profile(),
      InstallPWA(browser()->profile(), GURL("http://www.example.com")));
  const auto renderer_id = app_browser->tab_strip_model()
                               ->GetWebContentsAt(0)
                               ->GetPrimaryMainFrame()
                               ->GetGlobalId();

  PostNotifyStateChanged(
      renderer_id,
      /*label=*/"testinglabel",
      content::ContentBrowserClient::MultiCaptureChanged::kStarted);

  std::string expected_notifier_id =
      NotificationIdContainsLabel() ? "testinglabel" : "www.example.com";
  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id))));

  PostNotifyStateChanged(
      renderer_id,
      /*label=*/"testinglabel",
      content::ContentBrowserClient::MultiCaptureChanged::kStopped);
  WaitUntilDisplayNotificationCount(/*display_count=*/0u);
}

IN_PROC_BROWSER_TEST_F(MultiCaptureNotificationTest,
                       CalledFromAppMultipleRequestsNotificationsAreShown) {
  Browser* app_browser_1 = web_app::LaunchWebAppBrowserAndWait(
      browser()->profile(),
      InstallPWA(browser()->profile(), GURL("http://www.example1.com")));
  Browser* app_browser_2 = web_app::LaunchWebAppBrowserAndWait(
      browser()->profile(),
      InstallPWA(browser()->profile(), GURL("http://www.example2.com")));
  const auto renderer_id_1 = app_browser_1->tab_strip_model()
                                 ->GetWebContentsAt(0)
                                 ->GetPrimaryMainFrame()
                                 ->GetGlobalId();
  const auto renderer_id_2 = app_browser_2->tab_strip_model()
                                 ->GetWebContentsAt(0)
                                 ->GetPrimaryMainFrame()
                                 ->GetGlobalId();

  std::string expected_notifier_id_1 =
      NotificationIdContainsLabel() ? "testinglabel1" : "www.example1.com";
  PostNotifyStateChanged(
      renderer_id_1,
      /*label=*/"testinglabel1",
      content::ContentBrowserClient::MultiCaptureChanged::kStarted);
  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id_1))));

  std::string expected_notifier_id_2 =
      NotificationIdContainsLabel() ? "testinglabel2" : "www.example2.com";
  PostNotifyStateChanged(
      renderer_id_2,
      /*label=*/"testinglabel2",
      content::ContentBrowserClient::MultiCaptureChanged::kStarted);
  WaitUntilDisplayNotificationCount(/*display_count=*/2u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id_1)),
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id_2))));

  PostNotifyStateChanged(
      renderer_id_2,
      /*label=*/"testinglabel2",
      content::ContentBrowserClient::MultiCaptureChanged::kStopped);
  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  EXPECT_THAT(GetAllNotifications(),
              testing::UnorderedElementsAre(
                  testing::AllOf(testing::HasSubstr("multi_capture"),
                                 testing::HasSubstr(expected_notifier_id_1))));

  PostNotifyStateChanged(
      renderer_id_1,
      /*label=*/"testinglabel1",
      content::ContentBrowserClient::MultiCaptureChanged::kStopped);
  WaitUntilDisplayNotificationCount(/*display_count=*/0u);
}

class MultiScreenCaptureInIsolatedWebAppBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<bool> {
 public:
  MultiScreenCaptureInIsolatedWebAppBrowserTest()
      : with_strict_csp_(GetParam()) {
    scoped_feature_list_.InitFromCommandLine(
        /*enable_features=*/
        "GetAllScreensMedia",
        /*disable_features=*/"");
    app_ = CreateIsolatedWebApp(/*html_text=*/"GetAllScreensMedia allowed");
  }

  MultiScreenCaptureInIsolatedWebAppBrowserTest(
      const MultiScreenCaptureInIsolatedWebAppBrowserTest&) = delete;
  MultiScreenCaptureInIsolatedWebAppBrowserTest& operator=(
      const MultiScreenCaptureInIsolatedWebAppBrowserTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (!SupportsDisplaySetting()) {
      GTEST_SKIP();
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    web_app::IsolatedWebAppBrowserTestHarness::
        SetUpInProcessBrowserTestFixture();

    // For Ash, and end2end test is possible because the policy value can be set
    // up before the keyed service can cache the value.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    SetAllowedOriginsPolicy(/*allow_listed_origins=*/{
        "isolated-app://" + app_->web_bundle_id().id()});
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // For Lacros, a complete end2end test is not possible because Ash is
    // started long before this test is run and therefore the keyed service
    // backs up the policy value before this test set it up.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    capture_policy::SetMultiCaptureServiceForTesting(
        &mock_multi_capture_service_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> CreateIsolatedWebApp(
      const std::string html_text) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto builder = web_app::IsolatedWebAppBuilder(
        web_app::ManifestBuilder().SetName("app-3.0.4").SetVersion("3.0.4"));

    std::string csp;
    if (with_strict_csp_) {
      csp = R"(
        <meta http-equiv="Content-Security-Policy"
              content="object-src 'none'; base-uri 'none';
              script-src 'strict-dynamic'
              'sha256-9kZur2gwMdyfcGTQxLkOGeJuiPDxPqd1z9n1YSaCirY='
              'sha256-qm/4Jb2mtycc7MTgVEj+rdFrTTJkCOTsMl1tSRaAkLc=';
              require-trusted-types-for 'script';" />
      )";
    }

    builder.AddHtml("/", "<head>" + csp + R"(
            <script type="text/javascript" src="/test_functions.js"
              integrity="sha256-9kZur2gwMdyfcGTQxLkOGeJuiPDxPqd1z9n1YSaCirY=">
            </script>
            <script type="text/javascript"
              src="/get_all_screens_media_functions.js"
              integrity="sha256-qm/4Jb2mtycc7MTgVEj+rdFrTTJkCOTsMl1tSRaAkLc=">
            </script>
            <title>3.0.4</title>
          </head>
          <body>
            <h1>)" + html_text +
                             "</h1></body>)");
    builder.AddFileFromDisk("test_functions.js",
                            GetSourceDir().Append(FILE_PATH_LITERAL(
                                "chrome/test/data/webrtc/test_functions.js")));
    builder.AddFileFromDisk(
        "get_all_screens_media_functions.js",
        GetSourceDir().Append(FILE_PATH_LITERAL(
            "chrome/test/data/webrtc/get_all_screens_media_functions.js")));
    auto app = builder.BuildBundle();
    app->TrustSigningKey();
    return app;
  }

  base::FilePath GetSourceDir() {
    base::FilePath source_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir);
    return source_dir;
  }

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

 protected:
  const bool with_strict_csp_;
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  testing::StrictMock<MockMultiCaptureService> mock_multi_capture_service_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         MultiScreenCaptureInIsolatedWebAppBrowserTest,
                         // Determines whether the IWA uses explicit strict CSP.
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(MultiScreenCaptureInIsolatedWebAppBrowserTest,
                       GetAllScreensMediaSuccessful) {
  SetScreens(/*screen_count=*/1u);
  web_app::IsolatedWebAppUrlInfo url_info = app_->Install(profile()).value();
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(mock_multi_capture_service_,
              IsMultiCaptureAllowed(url_info.origin().GetURL(), testing::_))
      .WillOnce(testing::Invoke(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(
      content::WebContents::FromRenderFrameHost(app_frame), stream_ids,
      track_ids, &error_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(1u, track_ids.size());
}

IN_PROC_BROWSER_TEST_P(MultiScreenCaptureInIsolatedWebAppBrowserTest,
                       GetAllScreensMediaDenied) {
  SetScreens(/*screen_count=*/1u);
  auto denied_app =
      CreateIsolatedWebApp(/*html_text=*/"GetAllScreensMedia denied");

  web_app::IsolatedWebAppUrlInfo url_info =
      denied_app->Install(profile()).value();
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(mock_multi_capture_service_,
              IsMultiCaptureAllowed(url_info.origin().GetURL(), testing::_))
      .WillOnce(testing::Invoke(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          }));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(
      content::WebContents::FromRenderFrameHost(app_frame), stream_ids,
      track_ids, &error_name);
  EXPECT_FALSE(result);
}
