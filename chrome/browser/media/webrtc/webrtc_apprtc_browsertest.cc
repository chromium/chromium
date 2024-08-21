// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_responder.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/python_utils.h"
#include "ui/gl/gl_switches.h"

const char kTitlePageOfAppEngineAdminPage[] = "Instances";

const char kIsApprtcCallUpJavascript[] =
    "var remoteVideo = document.querySelector('#remote-video');"
    "var remoteVideoActive ="
    "    remoteVideo != null &&"
    "    remoteVideo.classList.contains('active');"
    "remoteVideoActive.toString();";

// WebRTC-AppRTC integration test. Requires a real webcam and microphone
// on the running system. This test is not meant to run in the main browser
// test suite since normal tester machines do not have webcams.
//
// This test will bring up a AppRTC instance on localhost and verify that the
// call gets up when connecting to the same room from two tabs in a browser.
//
// TODO(b/246519185) - Py3 incompatible, decide if to keep test.
class WebRtcApprtcBrowserTest : public WebRtcTestBase {
 public:
  WebRtcApprtcBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
    command_line->RemoveSwitch(switches::kUseFakeDeviceForMediaStream);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
  }

  void TearDown() override {
    // Kill any processes we may have brought up. Note: this isn't perfect,
    // especially if the test hangs or if we're on Windows.
    LOG(INFO) << "Entering TearDown";
    if (dev_appserver_.IsValid())
      dev_appserver_.Terminate(0, false);
    if (collider_server_.IsValid())
      collider_server_.Terminate(0, false);
    LOG(INFO) << "Exiting TearDown";
  }

 protected:
  bool LaunchApprtcInstanceOnLocalhost(const std::string& port) {
    base::FilePath appengine_dev_appserver = GetSourceDir().Append(
        FILE_PATH_LITERAL("third_party/webrtc/rtc_tools/testing/browsertest/"
                          "apprtc/temp/google-cloud-sdk/bin/dev_appserver.py"));
    if (!base::PathExists(appengine_dev_appserver)) {
      LOG(ERROR) << "Missing appengine sdk at " <<
          appengine_dev_appserver.value() << ".\n" <<
          test::kAdviseOnGclientSolution;
      return false;
    }

    base::FilePath apprtc_dir = GetSourceDir().Append(
        FILE_PATH_LITERAL("third_party/webrtc/rtc_tools/testing/"
                          "browsertest/apprtc/out/app_engine"));
    if (!base::PathExists(apprtc_dir)) {
      LOG(ERROR) << "Missing AppRTC AppEngine app at " <<
          apprtc_dir.value() << ".\n" << test::kAdviseOnGclientSolution;
      return false;
    }
    if (!base::PathExists(apprtc_dir.Append(FILE_PATH_LITERAL("app.yaml")))) {
      LOG(ERROR) << "The AppRTC AppEngine app at " << apprtc_dir.value()
                 << " appears to have not been built."
                 << "This should have been done by webrtc.DEPS scripts.";
      return false;
    }

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    EXPECT_TRUE(GetPython3Command(&command_line));

    command_line.AppendArgPath(appengine_dev_appserver);
    command_line.AppendArgPath(apprtc_dir);
    command_line.AppendArg("--port=" + port);
    command_line.AppendArg("--admin_port=9998");
    command_line.AppendArg("--skip_sdk_update_check");
    command_line.AppendArg("--clear_datastore=yes");

    DVLOG(1) << "Running " << command_line.GetCommandLineString();
    dev_appserver_ = base::LaunchProcess(command_line, base::LaunchOptions());
    return dev_appserver_.IsValid();
  }

  bool LaunchColliderOnLocalHost(const std::string& apprtc_url,
                                 const std::string& collider_port) {
    // The go workspace should be created, and collidermain built, at the
    // runhooks stage when webrtc.DEPS/build_apprtc_collider.py runs.
#if BUILDFLAG(IS_WIN)
    base::FilePath collider_server = GetSourceDir().Append(
        FILE_PATH_LITERAL("third_party/webrtc/rtc_tools/testing/"
                          "browsertest/collider/collidermain.exe"));
#else
    base::FilePath collider_server = GetSourceDir().Append(
        FILE_PATH_LITERAL("third_party/webrtc/rtc_tools/testing/"
                          "browsertest/collider/collidermain"));
#endif
    if (!base::PathExists(collider_server)) {
      LOG(ERROR) << "Missing Collider server binary at " <<
          collider_server.value() << ".\n" << test::kAdviseOnGclientSolution;
      return false;
    }

    base::CommandLine command_line(collider_server);

    command_line.AppendArg("-tls=false");
    command_line.AppendArg("-port=" + collider_port);
    command_line.AppendArg("-room-server=" + apprtc_url);

    DVLOG(1) << "Running " << command_line.GetCommandLineString();
    collider_server_ = base::LaunchProcess(command_line, base::LaunchOptions());
    return collider_server_.IsValid();
  }

  bool LocalApprtcInstanceIsUp() {
    // Load the admin page and see if we manage to load it right.
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("http://localhost:9998")));
    content::WebContents* tab_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string javascript = "document.title";
    return content::EvalJs(tab_contents, javascript) ==
           kTitlePageOfAppEngineAdminPage;
  }

  bool WaitForCallToComeUp(content::WebContents* tab_contents) {
    return test::PollingWaitUntil(kIsApprtcCallUpJavascript, "true",
                                  tab_contents);
  }

  bool EvalInJavascriptFile(content::WebContents* tab_contents,
                            const base::FilePath& path) {
    std::string javascript;
    if (!ReadFileToString(path, &javascript)) {
      LOG(ERROR) << "Missing javascript code at " << path.value() << ".";
      return false;
    }

    if (!content::ExecJs(tab_contents, javascript)) {
      LOG(ERROR) << "Failed to execute the following javascript: " <<
          javascript;
      return false;
    }
    return true;
  }

  bool DetectLocalVideoPlaying(content::WebContents* tab_contents) {
    // The remote video tag is called "local-video" in the AppRTC code.
    return DetectVideoPlaying(tab_contents, "local-video");
  }

  bool DetectRemoteVideoPlaying(content::WebContents* tab_contents) {
    // The remote video tag is called "remote-video" in the AppRTC code.
    return DetectVideoPlaying(tab_contents, "remote-video");
  }

  bool DetectVideoPlaying(content::WebContents* tab_contents,
                          const std::string& video_tag) {
    if (!EvalInJavascriptFile(tab_contents, GetSourceDir().Append(
        FILE_PATH_LITERAL("chrome/test/data/webrtc/test_functions.js"))))
      return false;
    if (!EvalInJavascriptFile(tab_contents, GetSourceDir().Append(
        FILE_PATH_LITERAL("chrome/test/data/webrtc/video_detector.js"))))
      return false;

    StartDetectingVideo(tab_contents, video_tag);
    WaitForVideoToPlay(tab_contents);
    return true;
  }

  base::FilePath GetSourceDir() {
    base::FilePath source_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir);
    return source_dir;
  }

 private:
  base::Process dev_appserver_;
  base::Process collider_server_;
};

IN_PROC_BROWSER_TEST_F(WebRtcApprtcBrowserTest, MANUAL_WorksOnApprtc) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  DetectErrorsInJavaScript();
  ASSERT_TRUE(LaunchApprtcInstanceOnLocalhost("9999"));
  ASSERT_TRUE(LaunchColliderOnLocalHost("http://localhost:9999", "8089"));
  while (!LocalApprtcInstanceIsUp())
    DVLOG(1) << "Waiting for AppRTC to come up...";

  GURL room_url = GURL("http://localhost:9999/r/some_room"
                       "?wshpp=localhost:8089&wstls=false");

  // Set up the left tab.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  content::WebContents* left_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager::FromWebContents(left_tab)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);
  InfoBarResponder left_infobar_responder(
      infobars::ContentInfoBarManager::FromWebContents(left_tab),
      InfoBarResponder::ACCEPT);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), room_url));

  // Wait for the local video to start playing. This is needed, because opening
  // a new tab too quickly, by sending the current tab to the background, can
  // lead to the request for starting the video capture in the current tab to
  // not get sent before it comes back to the foreground (which in this test
  // case is never).
  ASSERT_TRUE(DetectLocalVideoPlaying(left_tab));

  // Set up the right tab.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  content::WebContents* right_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager::FromWebContents(right_tab)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);
  InfoBarResponder right_infobar_responder(
      infobars::ContentInfoBarManager::FromWebContents(right_tab),
      InfoBarResponder::ACCEPT);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), room_url));

  ASSERT_TRUE(WaitForCallToComeUp(left_tab));
  ASSERT_TRUE(WaitForCallToComeUp(right_tab));

  ASSERT_TRUE(DetectRemoteVideoPlaying(left_tab));
  ASSERT_TRUE(DetectRemoteVideoPlaying(right_tab));

  chrome::CloseWebContents(browser(), left_tab, false);
  chrome::CloseWebContents(browser(), right_tab, false);
}
