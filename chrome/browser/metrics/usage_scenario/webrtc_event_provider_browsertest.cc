// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/webrtc_event_provider.h"

#include <memory>

#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_tracker.h"
#include "content/public/test/browser_test.h"

static const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

class WebRtcEventProviderBrowserTest : public WebRtcTestBase {
 public:
  WebRtcEventProviderBrowserTest() = default;
  ~WebRtcEventProviderBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void SetUpOnMainThread() override {
    web_rtc_event_provider_ =
        std::make_unique<WebRtcEventProvider>(&usage_scenario_data_store_);
  }

  UsageScenarioDataStoreImpl* data_store() {
    return &usage_scenario_data_store_;
  }

  UsageScenarioDataStoreImpl usage_scenario_data_store_;
  std::unique_ptr<WebRtcEventProvider> web_rtc_event_provider_;
};

// A test that opens a WebRTC connection between 2 tabs.
IN_PROC_BROWSER_TEST_F(WebRtcEventProviderBrowserTest, DetectWebRtc) {
  // There are DCHECKs in the data store that ensure you don't receive
  // tab-related when there are no tabs.
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();

  // Start the server that serves the test page.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the 2 pages that will communicate with each others.
  content::WebContents* web_contents_1 =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  SetupPeerconnectionWithLocalStream(web_contents_1);
  content::WebContents* web_contents_2 =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  SetupPeerconnectionWithLocalStream(web_contents_2);

  // Start the WebRTC connection.
  NegotiateCall(web_contents_1, web_contents_2);

  // The WebRTC connections in both tabs are detected.
  EXPECT_EQ(data_store()->webrtc_open_connection_count_for_testing(), 2);

  // Close the connection between the 2 tabs.
  HangUp(web_contents_1);
  HangUp(web_contents_2);

  // The event provider no longer detects any open connections.
  EXPECT_EQ(data_store()->webrtc_open_connection_count_for_testing(), 0);
}

// Tests that not explicitly closing the WebRTC connections is handled
// correctly (Doesn't DCHECKs) by WebRtcEventProvider::RenderProcessExited().
IN_PROC_BROWSER_TEST_F(WebRtcEventProviderBrowserTest, RenderProcessExited) {
  // There are DCHECKs in the data store that ensure you don't receive
  // tab-related when there are no tabs.
  data_store()->OnTabAdded();
  data_store()->OnTabAdded();

  // Start the server that serves the test page.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the 2 pages that will communicate with each others.
  content::WebContents* web_contents_1 =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  SetupPeerconnectionWithLocalStream(web_contents_1);
  content::WebContents* web_contents_2 =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  SetupPeerconnectionWithLocalStream(web_contents_2);

  // Start the WebRTC connection.
  NegotiateCall(web_contents_1, web_contents_2);

  // The WebRTC connections in both tabs are detected.
  EXPECT_EQ(data_store()->webrtc_open_connection_count_for_testing(), 2);
}
