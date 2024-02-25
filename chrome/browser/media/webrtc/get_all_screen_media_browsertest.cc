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
#include "third_party/blink/public/common/features.h"
#include "ui/display/test/display_manager_test_api.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "chrome/browser/notifications/notification_display_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/message_center.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

struct GetAllScreensMediaTestParameters {
  std::string base_page;
  bool expected_csp_acceptable;
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

class ContentBrowserClientMock : public ChromeContentBrowserClient {
 public:
  bool IsGetAllScreensMediaAllowed(content::BrowserContext* context,
                                   const url::Origin& origin) override {
    return is_get_display_media_set_select_all_screens_allowed_;
  }

  void SetIsGetAllScreensMediaAllowed(bool is_allowed) {
    is_get_display_media_set_select_all_screens_allowed_ = is_allowed;
  }

 private:
  bool is_get_display_media_set_select_all_screens_allowed_ = true;
};

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
    WebRtcTestBase::SetUpOnMainThread();
    browser_client_ = std::make_unique<ContentBrowserClientMock>();
    content::SetBrowserClientForTesting(browser_client_.get());
    ASSERT_TRUE(embedded_test_server()->Start());
    contents_ = OpenTestPageInNewTab(base_page_);
    DCHECK(contents_);
  }

  void SetScreens(size_t screen_count) {
    // This part of the test only works on ChromeOS.
    std::stringstream screens;
    for (size_t screen_index = 0; screen_index + 1 < screen_count;
         screen_index++) {
      // Each entry in this comma separated list corresponds to a screen
      // specification following the format defined in
      // |ManagedDisplayInfo::CreateFromSpec|.
      // The used specification simulates screens with resolution 800x800
      // at the host coordinates (screen_index * 800, 0).
      screens << screen_index * 640 << "+0-640x480,";
    }
    if (screen_count != 0) {
      screens << (screen_count - 1) * 640 << "+0-640x480";
    }
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay(screens.str());
  }

 protected:
  raw_ptr<content::WebContents, DanglingUntriaged> contents_ = nullptr;
  std::unique_ptr<ContentBrowserClientMock> browser_client_;

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
};

INSTANTIATE_TEST_SUITE_P(
    ,
    GetAllScreensMediaBrowserTest,
    ::testing::ValuesIn(std::vector<GetAllScreensMediaTestParameters>{
        {/*base_page=*/"/webrtc/webrtc_getallscreensmedia_valid_csp_test.html",
         /*expected_csp_acceptable=*/true},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_no_object_source_test.html",
         /*expected_csp_acceptable=*/false},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_no_base_uri_test.html",
         /*expected_csp_acceptable=*/false},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_no_script_source_test.html",
         /*expected_csp_acceptable=*/false},
        {/*base_page=*/"/webrtc/"
                       "webrtc_getallscreensmedia_no_trusted_types_test.html",
         /*expected_csp_acceptable=*/false},
    }));

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       GetAllScreensMediaSingleScreenAccessBasedOnCSP) {
  SetScreens(/*screen_count=*/1u);
  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(contents_, stream_ids,
                                                     track_ids, &error_name);
  if (GetParam().expected_csp_acceptable) {
    EXPECT_TRUE(result);
    EXPECT_EQ(1u, track_ids.size());
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ("NotAllowedError", error_name);
  }
}

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       GetAllScreensMediaNoScreenSuccessIfStrictCSP) {
  SetScreens(/*screen_count=*/0u);
  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(contents_, stream_ids,
                                                     track_ids, &error_name);
  if (GetParam().expected_csp_acceptable) {
    EXPECT_TRUE(result);
    // If no screen is attached to a device, the |DisplayManager| will add a
    // default device. This same behavior is used in other places in Chrome that
    // handle multiple screens (e.g. in JS window.getScreenDetails() API) and
    // getAllScreensMedia will follow the same convention.
    EXPECT_EQ(1u, stream_ids.size());
    EXPECT_EQ(1u, track_ids.size());
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ("NotAllowedError", error_name);
  }
}

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       GetAllScreensMediaMultipleScreensSuccessIfStrictCSP) {
  base::AddTagToTestResult("feature_id",
                           "screenplay-f3601ae4-bff7-495a-a51f-3c0997a46445");
  SetScreens(/*screen_count=*/5u);
  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(contents_, stream_ids,
                                                     track_ids, &error_name);
  if (GetParam().expected_csp_acceptable) {
    EXPECT_TRUE(result);
    // TODO(crbug.com/1404274): Adapt this test if a decision is made on whether
    // stream ids shall be shared or unique.
    EXPECT_EQ(1u, stream_ids.size());
    EXPECT_EQ(5u, track_ids.size());
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ("NotAllowedError", error_name);
  }
}

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       TrackContainsScreenDetailedIfStrictCSP) {
  SetScreens(/*screen_count=*/1u);
  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  const bool result = RunGetAllScreensMediaAndGetIds(contents_, stream_ids,
                                                     track_ids, &error_name);
  if (GetParam().expected_csp_acceptable) {
    EXPECT_TRUE(result);
    EXPECT_TRUE(result);
    ASSERT_EQ(1u, stream_ids.size());
    ASSERT_EQ(1u, track_ids.size());

    EXPECT_TRUE(CheckScreenDetailedExists(contents_, *track_ids.begin()));
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ("NotAllowedError", error_name);
  }
}

IN_PROC_BROWSER_TEST_P(GetAllScreensMediaBrowserTest,
                       AutoSelectAllScreensNotAllowedByAdminPolicy) {
  SetScreens(/*screen_count=*/1u);
  browser_client_->SetIsGetAllScreensMediaAllowed(
      /*is_allowed=*/false);
  std::set<std::string> stream_ids;
  std::set<std::string> track_ids;
  std::string error_name;
  EXPECT_FALSE(RunGetAllScreensMediaAndGetIds(contents_, stream_ids, track_ids,
                                              &error_name));
  EXPECT_EQ("NotAllowedError", error_name);
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

  ASSERT_EQ(nullptr, Run(method1_));
  ASSERT_EQ(nullptr, Run(method2_));
  ASSERT_EQ(nullptr, ProgrammaticallyStop(method1_));

  EXPECT_EQ(false, AreAllTracksLive(method1_));
  EXPECT_EQ(true, AreAllTracksLive(method2_));
}

// Identical to StoppingOneDoesNotStopTheOther other than that this following
// test stops the second-started method first.
IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    ProgrammaticallyStoppingOneDoesNotStopTheOtherInverseOrder) {
  SetScreens(/*screen_count=*/1u);

  ASSERT_EQ(nullptr, Run(method1_));
  ASSERT_EQ(nullptr, Run(method2_));
  ASSERT_EQ(nullptr, ProgrammaticallyStop(method2_));

  EXPECT_EQ(true, AreAllTracksLive(method1_));
  EXPECT_EQ(false, AreAllTracksLive(method2_));
}

// TODO(crbug.com/1479984): re-enable once the bug is fixed.
IN_PROC_BROWSER_TEST_P(
    InteractionBetweenGetAllScreensMediaAndGetDisplayMediaTest,
    DISABLED_UserStoppingGetDisplayMediaDoesNotStopGetAllScreensMedia) {
  SetScreens(/*screen_count=*/1u);

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
  EXPECT_EQ(true, AreAllTracksLive("getAllScreensMedia"));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
