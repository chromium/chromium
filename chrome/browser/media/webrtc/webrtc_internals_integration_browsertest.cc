// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using webrtc_event_logging::WebRtcEventLogManager;

namespace {
const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";
}

class WebRTCInternalsIntegrationBrowserTest : public WebRtcTestBase {
 public:
  ~WebRTCInternalsIntegrationBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(local_logs_dir_.CreateUniqueTempDir());
    }
    command_line->AppendSwitchASCII(switches::kWebRtcLocalEventLogging,
                                    local_logs_dir_.GetPath().MaybeAsASCII());
  }

  // To avoid flaky tests, we need to synchronize with WebRtcEventLogger's
  // internal task runners (if any exist) before we examine anything we
  // expect to be produced by WebRtcEventLogger (files, etc.).
  void WaitForEventLogProcessing() {
    WebRtcEventLogManager* manager = WebRtcEventLogManager::GetInstance();
    ASSERT_TRUE(manager);

    base::RunLoop run_loop;
    manager->PostNullTaskForTesting(run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  bool IsDirectoryEmpty(const base::FilePath& path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::IsDirectoryEmpty(path);
  }

  base::ScopedTempDir local_logs_dir_;
};

// Sheriff 2022-04-18: disabling due to flakiness: crbug/1317072
IN_PROC_BROWSER_TEST_F(WebRTCInternalsIntegrationBrowserTest,
                       DISABLED_IntegrationWithWebRtcEventLogger) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

  ASSERT_TRUE(IsDirectoryEmpty(local_logs_dir_.GetPath()));  // Sanity on test.

  // Local WebRTC event logging turned on from command line using the
  // kWebRtcLocalEventLogging flag. When we set up a peer connection, it
  // will be logged to a file under |local_logs_dir_|.
  SetupPeerconnectionWithLocalStream(tab);

  WaitForEventLogProcessing();

  EXPECT_FALSE(IsDirectoryEmpty(local_logs_dir_.GetPath()));
}
