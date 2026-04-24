// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/media/webrtc/rtc_diagnostic_logging_utils.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using webrtc_event_logging::WebRtcEventLogManager;
using webrtc_event_logging::WebRtcEventLogPeerConnectionKey;
using webrtc_event_logging::WebRtcRemoteEventLogsObserver;

namespace {

class MockRemoteLogsObserver : public WebRtcRemoteEventLogsObserver {
 public:
  MockRemoteLogsObserver() = default;
  ~MockRemoteLogsObserver() override = default;

  MOCK_METHOD3(OnRemoteLogStarted,
               void(WebRtcEventLogPeerConnectionKey key,
                    const base::FilePath& file_path,
                    int output_period_ms));

  MOCK_METHOD1(OnRemoteLogStopped, void(WebRtcEventLogPeerConnectionKey key));
};

constexpr char kSessionId[] = "id";

}  // namespace

class WebRtcDiagnosticLogCollectionAllowedForOriginsPolicyTest
    : public policy::PolicyTest {
 public:
  WebRtcDiagnosticLogCollectionAllowedForOriginsPolicyTest() = default;
  ~WebRtcDiagnosticLogCollectionAllowedForOriginsPolicyTest() override =
      default;

  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  void SetPolicy(const std::string& key, base::Value value) {
    policies_.Set(key, policy::POLICY_LEVEL_MANDATORY,
                  policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                  std::move(value), nullptr);
    provider_.UpdateChromePolicy(policies_);
  }

  void SetRemoteLogsObserver(WebRtcRemoteEventLogsObserver* observer) {
    base::test::TestFuture<void> future;
    WebRtcEventLogManager::GetInstance()->SetRemoteLogsObserver(
        observer, base::BindPostTaskToCurrentDefault(future.GetCallback()));
    EXPECT_TRUE(future.Wait());
  }

  WebRtcLoggingController* GetController(content::RenderFrameHost* frame) {
    return WebRtcLoggingController::FromRenderProcessHost(frame->GetProcess());
  }

  void SetupOriginsPolicy() {
    allowed_url_ =
        embedded_test_server()->GetURL("allowed.com", "/title1.html");
    blocked_url_ =
        embedded_test_server()->GetURL("blocked.com", "/title1.html");

    const std::string allowed_origin =
        url::Origin::Create(allowed_url_).Serialize();

    base::Value allowed_origins(base::Value::Type::LIST);
    allowed_origins.GetList().Append(allowed_origin);
    SetPolicy(policy::key::kWebRtcDiagnosticLogCollectionAllowedForOrigins,
              std::move(allowed_origins));
    SetPolicy(policy::key::kWebRtcEventLogCollectionAllowed, base::Value(true));
    SetPolicy(policy::key::kWebRtcTextLogCollectionAllowed, base::Value(true));
  }

  content::RenderFrameHost* SetupLoggingForUrl(const GURL& url, int lid) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    if (!content::NavigateToURL(web_contents, url)) {
      return nullptr;
    }

    content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
    WebRtcLoggingController::AttachToRenderProcessHost(frame->GetProcess());

    base::test::TestFuture<const std::string&> start_rtc_diag_future;
    rtc_diagnostic_logging::StartRtcDiagnosticLogging(
        *frame, /*should_upload_on_stop=*/true, {},
        base::BindPostTaskToCurrentDefault(
            start_rtc_diag_future.GetCallback()));
    EXPECT_TRUE(start_rtc_diag_future.Wait());

    WebRtcEventLogManager::GetInstance()->OnPeerConnectionAdded(
        frame->GetGlobalId(), lid, frame->GetProcess()->GetProcess().Pid(),
        url.spec(), "");
    WebRtcEventLogManager::GetInstance()->OnPeerConnectionSessionIdSet(
        frame->GetGlobalId(), lid, kSessionId, base::DoNothing());

    return frame;
  }

 protected:
  GURL allowed_url_;
  GURL blocked_url_;

 private:
  policy::PolicyMap policies_;
};

IN_PROC_BROWSER_TEST_F(WebRtcDiagnosticLogCollectionAllowedForOriginsPolicyTest,
                       AllowedOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetupOriginsPolicy();

  MockRemoteLogsObserver observer;
  SetRemoteLogsObserver(&observer);

  content::RenderFrameHost* frame =
      SetupLoggingForUrl(allowed_url_, /*lid=*/123);
  ASSERT_TRUE(frame);

  // Verify text logging started.
  EXPECT_FALSE(GetController(frame)->GetLogMessageCallback().is_null());

  // Verify event logging started.
  EXPECT_CALL(observer, OnRemoteLogStarted(testing::_, testing::_, testing::_));
  rtc_diagnostic_logging::StartRtcPeerConnectionEventDiagnosticLogging(
      *frame, kSessionId, base::DoNothing());

  SetRemoteLogsObserver(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebRtcDiagnosticLogCollectionAllowedForOriginsPolicyTest,
                       BlockedOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetupOriginsPolicy();

  MockRemoteLogsObserver observer;
  SetRemoteLogsObserver(&observer);

  content::RenderFrameHost* frame =
      SetupLoggingForUrl(blocked_url_, /*lid=*/456);
  ASSERT_TRUE(frame);

  // Verify text logging did not start.
  EXPECT_TRUE(GetController(frame)->GetLogMessageCallback().is_null());

  // Verify event logging did not start.
  EXPECT_CALL(observer, OnRemoteLogStarted(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<void> future;
  rtc_diagnostic_logging::StartRtcPeerConnectionEventDiagnosticLogging(
      *frame, kSessionId,
      base::BindPostTaskToCurrentDefault(future.GetCallback()));
  EXPECT_TRUE(future.Wait());

  SetRemoteLogsObserver(nullptr);
}
