// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/test/browser_test.h"

class WebrtcLoggingPrivateApiBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  WebrtcLoggingPrivateApiBrowserTest() = default;

  WebrtcLoggingPrivateApiBrowserTest(
      const WebrtcLoggingPrivateApiBrowserTest&) = delete;
  WebrtcLoggingPrivateApiBrowserTest& operator=(
      const WebrtcLoggingPrivateApiBrowserTest&) = delete;

  ~WebrtcLoggingPrivateApiBrowserTest() override = default;

  base::FilePath webrtc_logs_path() {
    return webrtc_logging::TextLogList::
        GetWebRtcLogDirectoryForBrowserContextPath(profile()->GetPath());
  }
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestGetLogsDirectoryCreatesWebRtcLogsDirectory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::PathExists(webrtc_logs_path()));
  ASSERT_TRUE(RunExtensionTest(
      "api_test/webrtc_logging_private/get_logs_directory",
      {.custom_arg = "test_without_directory", .launch_as_platform_app = true}))
      << message_;
  ASSERT_TRUE(base::PathExists(webrtc_logs_path()));
  ASSERT_TRUE(base::IsDirectoryEmpty(webrtc_logs_path()));
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestGetLogsDirectoryReadsFiles) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::CreateDirectory(webrtc_logs_path()));
  base::FilePath test_file_path = webrtc_logs_path().AppendASCII("test.file");
  std::string contents = "test file contents";
  ASSERT_TRUE(base::WriteFile(test_file_path, contents));
  ASSERT_TRUE(
      RunExtensionTest("api_test/webrtc_logging_private/get_logs_directory",
                       {.custom_arg = "test_with_file_in_directory",
                        .launch_as_platform_app = true}))
      << message_;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestNoGetLogsDirectoryPermissionsFromHangoutsExtension) {
  ASSERT_TRUE(RunExtensionTest(
      "api_test/webrtc_logging_private/no_get_logs_directory_permissions", {},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestStartAudioDebugRecordingsForWebviewFromApp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAudioDebugRecordingsFromExtension);
  ASSERT_TRUE(
      RunExtensionTest("api_test/webrtc_logging_private/audio_debug/"
                       "start_audio_debug_recordings_for_webview_from_app",
                       {.launch_as_platform_app = true}))
      << message_;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiBrowserTest,
    TestStartAudioDebugRecordingsForWebviewFromAppWithoutSwitch) {
  ASSERT_TRUE(
      RunExtensionTest("api_test/webrtc_logging_private/audio_debug/"
                       "start_audio_debug_recordings_for_webview_from_app",
                       {.launch_as_platform_app = true}))
      << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest, TestStartStopStart) {
  ASSERT_TRUE(
      RunExtensionTest("api_test/webrtc_logging_private/start_stop_start",
                       {.launch_as_platform_app = true}))
      << message_;
}
