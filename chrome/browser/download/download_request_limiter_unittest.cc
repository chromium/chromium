// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#endif

using content::WebContents;

namespace {
enum TestingAction {
  ACCEPT,
  CANCEL,
  WAIT
};
}  // namespace

class DownloadRequestLimiterTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PermissionRequestManager::CreateForWebContents(web_contents());
    PermissionRequestManager* manager =
        PermissionRequestManager::FromWebContents(web_contents());
    mock_permission_prompt_factory_.reset(
        new MockPermissionPromptFactory(manager));

    UpdateExpectations(ACCEPT);
    cancel_count_ = continue_count_ = 0;
    download_request_limiter_ = new DownloadRequestLimiter();
  }

  void TearDown() override {
    mock_permission_prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CanDownload() {
    CanDownloadFor(web_contents());
  }

  void CanDownloadFor(WebContents* web_contents) {
    CanDownloadFor(web_contents, base::nullopt);
  }

  void CanDownloadFor(WebContents* web_contents,
                      base::Optional<url::Origin> origin) {
    download_request_limiter_->CanDownloadImpl(
        web_contents,
        "GET",  // request method
        std::move(origin),
        base::Bind(&DownloadRequestLimiterTest::ContinueDownload,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void OnUserInteraction(blink::WebInputEvent::Type type) {
    OnUserInteractionFor(web_contents(), type);
  }

  void OnUserInteractionFor(WebContents* web_contents,
                            blink::WebInputEvent::Type type) {
    DownloadRequestLimiter::TabDownloadState* state =
        download_request_limiter_->GetDownloadState(web_contents, false);
    if (state)
      state->DidGetUserInteraction(type);
  }

  void ExpectAndResetCounts(
      int expect_continues,
      int expect_cancels,
      int expect_asks,
      int line) {
    EXPECT_EQ(expect_continues, continue_count_) << "line " << line;
    EXPECT_EQ(expect_cancels, cancel_count_) << "line " << line;
    EXPECT_EQ(expect_asks, AskAllowCount()) << "line " << line;
    continue_count_ = cancel_count_ = 0;
    mock_permission_prompt_factory_->ResetCounts();
  }

  void UpdateContentSettings(WebContents* web_contents,
                             ContentSetting setting) {
    // Ensure a download state exists.
    download_request_limiter_->GetDownloadState(web_contents, true);
    SetHostContentSetting(web_contents, setting);
  }

 protected:
  void ContinueDownload(bool allow) {
    if (allow) {
      continue_count_++;
    } else {
      cancel_count_++;
    }
  }

  void SetHostContentSetting(WebContents* contents, ContentSetting setting) {
    SetHostContentSetting(contents, contents->GetURL(), setting);
  }

  void SetHostContentSetting(WebContents* contents,
                             const GURL& host,
                             ContentSetting setting) {
    HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(contents->GetBrowserContext()))
        ->SetContentSettingDefaultScope(
            host, GURL(), ContentSettingsType::AUTOMATIC_DOWNLOADS,
            std::string(), setting);
  }

  void LoadCompleted() {
    mock_permission_prompt_factory_->DocumentOnLoadCompletedInMainFrame();
  }

  int AskAllowCount() { return mock_permission_prompt_factory_->show_count(); }

  void UpdateExpectations(TestingAction action) {
    // Set expectations for PermissionRequestManager.
    PermissionRequestManager::AutoResponseType response_type =
        PermissionRequestManager::DISMISS;
    switch (action) {
      case ACCEPT:
        response_type = PermissionRequestManager::ACCEPT_ALL;
        break;
      case CANCEL:
        response_type = PermissionRequestManager::DENY_ALL;
        break;
      case WAIT:
        response_type = PermissionRequestManager::NONE;
        break;
    }
    mock_permission_prompt_factory_->set_response_type(response_type);
  }

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  // Number of times ContinueDownload was invoked.
  int continue_count_;

  // Number of times CancelDownload was invoked.
  int cancel_count_;

  std::unique_ptr<MockPermissionPromptFactory> mock_permission_prompt_factory_;
};

TEST_F(DownloadRequestLimiterTest, Allow) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // All tabs should initially start at ALLOW_ONE_DOWNLOAD and
  // DOWNLOAD_UI_DEFAULT.
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Ask if the tab can do a download. This moves to PROMPT_BEFORE_DOWNLOAD and
  // stays in DOWNLOAD_UI_DEFAULT.
  CanDownload();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
  // We should have been told we can download.
  ExpectAndResetCounts(1, 0, 0, __LINE__);

  // Ask again. This triggers asking the delegate for allow/disallow.
  UpdateExpectations(ACCEPT);
  CanDownload();
  // This should ask us if the download is allowed. We should have been told we
  // can download, and the UI state should reflect this.
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Ask again and make sure continue is invoked.
  CanDownload();
  // The state is at ALLOW_ALL_DOWNLOADS, which means the delegate shouldn't be
  // asked. We should have been told we can download.
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, ResetOnNavigation) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do two downloads, allowing the second so that we end up with allow all.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  UpdateExpectations(ACCEPT);
  CanDownload();
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  NavigateAndCommit(GURL("http://foo.com/bar2"));
  LoadCompleted();

  // We reset the UI state to DEFAULT on navigating prior to invoking a
  // download.
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);

  // Now we have triggered a download so the UI state reflects the
  // ALLOW_ALL_DOWNLOADS internal state.
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Do a user gesture, because we're at allow all, this shouldn't change the
  // state.
  OnUserInteraction(blink::WebInputEvent::kRawKeyDown);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Navigate to a completely different host, which should reset the state.
  NavigateAndCommit(GURL("http://fooey.com"));
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Do two downloads, blocking the second so that we end up with downloads not
  // allowed.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  UpdateExpectations(CANCEL);
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the not
  // allowed state, but reset the UI state.
  NavigateAndCommit(GURL("http://fooey.com/bar2"));
  LoadCompleted();

  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Try downloading and this should move to the blocked UI state.
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, RendererInitiated) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do one download so we end up in PROMPT.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Set up a renderer-initiated navigation to the same host.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foo.com/bar2"), web_contents()->GetMainFrame());
  LoadCompleted();

  // The state should not be reset.
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Renderer-initiated nav to a different host shouldn't reset the state.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://fooey.com/bar"), web_contents()->GetMainFrame());
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Set up a subframe. Navigations in the subframe shouldn't reset the state.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foo.com"), subframe);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foobargoo.com"), subframe);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Set up a blocked state.
  UpdateExpectations(CANCEL);
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // The state should not be reset on a renderer-initiated load to either the
  // same host or a different host, in either the main frame or the subframe.
  // The UI state goes to DEFAULT until an actual download is triggered.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://fooey.com/bar2"), web_contents()->GetMainFrame());
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foo.com/bar"), web_contents()->GetMainFrame());
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  rfh_tester =
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame());
  subframe = rfh_tester->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foo.com"), subframe);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foobarfoo.com"), subframe);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Browser-initiated navigation to a different host, which should reset the
  // state.
  NavigateAndCommit(GURL("http://foobar.com"));
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Set up an allow all state.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  UpdateExpectations(ACCEPT);
  CanDownload();
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // The state should not be reset on a pending renderer-initiated load to
  // the same host.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foobar.com/bar"), web_contents()->GetMainFrame());
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // The state should not be reset for a subframe nav to the same host.
  rfh_tester =
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame());
  subframe = rfh_tester->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foobar.com/bar"), subframe);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foobarfoo.com/"), subframe);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // But a pending load to a different host in the main frame should reset the
  // state.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foo.com"), web_contents()->GetMainFrame());
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}

// Test that history back will not change the tab download state if all the
// previous navigations are renderer-initiated.
TEST_F(DownloadRequestLimiterTest, HistoryBack) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do one download so we end up in PROMPT.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Renderer-initiated navigation to a different host shouldn't reset the
  // state.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foobar.com/bar"), web_contents()->GetMainFrame());
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // History back shouldn't reset the state, either.
  auto backward_navigation =
      content::NavigationSimulator::CreateHistoryNavigation(-1 /* Offset */,
                                                            web_contents());
  backward_navigation->Start();
  backward_navigation->Commit();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Browser-initiated navigation to a different host, which should reset the
  // state.
  NavigateAndCommit(GURL("http://foobar.com"));
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // History back should reset the state as it is going to a different host.
  backward_navigation = content::NavigationSimulator::CreateHistoryNavigation(
      -1 /* Offset */, web_contents());
  backward_navigation->Start();
  backward_navigation->Commit();
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}

// Tab download state shouldn't change when forward/back between to a
// renderer-initiated page.
TEST_F(DownloadRequestLimiterTest, HistoryForwardBack) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do one download so we end up in PROMPT.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Renderer-initiated navigation to a different host shouldn't reset the
  // state.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foobar.com/bar"), web_contents()->GetMainFrame());
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // History back shouldn't reset the state, either.
  auto backward_navigation =
      content::NavigationSimulator::CreateHistoryNavigation(-1 /* Offset */,
                                                            web_contents());
  backward_navigation->Start();
  backward_navigation->Commit();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // History forward shouldn't reset the state, as the host is encountered
  // before.
  auto forward_navigation =
      content::NavigationSimulator::CreateHistoryNavigation(1 /* Offset */,
                                                            web_contents());
  forward_navigation->Start();
  forward_navigation->Commit();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // History backward again, nothing should change.
  backward_navigation = content::NavigationSimulator::CreateHistoryNavigation(
      -1 /* Offset */, web_contents());
  backward_navigation->Start();
  backward_navigation->Commit();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_ResetOnUserGesture) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Do a user gesture with scroll, which should be ignored.
  OnUserInteraction(blink::WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
  // Do a user gesture with mouse click, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::kMouseDown);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Do a touch event, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::kTouchStart);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Do a user gesture with keyboard down, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::kRawKeyDown);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Ask twice, which triggers calling the delegate. Don't allow the download
  // so that we end up with not allowed.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  UpdateExpectations(CANCEL);
  CanDownload();
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
  ExpectAndResetCounts(0, 1, 1, __LINE__);

  // A user gesture now should NOT change the state.
  OnUserInteraction(blink::WebInputEvent::kMouseDown);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
  // And make sure we really can't download.
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  // And the state shouldn't have changed.
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, ResetOnReload) {
  // This is a regression test for https://crbug.com/110707.
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // A reload should keep us in ALLOW_ONE_DOWNLOAD.
  content::NavigationSimulator::Reload(web_contents());
  LoadCompleted();
  base::RunLoop().RunUntilIdle();
  ExpectAndResetCounts(0, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // If the user refreshes the page without responding to the prompt, pretend
  // like the refresh is the initial load: they get 1 free download (probably
  // the same as the actual initial load), then a prompt.
  UpdateExpectations(WAIT);

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 0, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  content::NavigationSimulator::Reload(web_contents());
  LoadCompleted();
  base::RunLoop().RunUntilIdle();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // After a browser-initiated reload, we return to the default UI state until
  // we see a download.
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  CanDownload();
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
  ExpectAndResetCounts(1, 0, 0, __LINE__);

  UpdateExpectations(CANCEL);
  CanDownload();
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
  ExpectAndResetCounts(0, 1, 1, __LINE__);

  content::NavigationSimulator::Reload(web_contents());
  LoadCompleted();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Reset back to default after a reload even if downloads are explicitly
  // blocked.
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // After the failed download, show the blocked UI.
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, RawWebContents) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());

  GURL url("http://foo.com/bar");
  web_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // DownloadRequestLimiter won't try to make a permission request or infobar
  // if there is no PermissionRequestManager, and we want to test that it will
  // CancelOnce() instead of prompting.
  ExpectAndResetCounts(0, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents.get()));

  // You get one freebie.
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents.get()));

  OnUserInteractionFor(web_contents.get(), blink::WebInputEvent::kTouchStart);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents.get()));

  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents.get()));

  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents.get()));

  OnUserInteractionFor(web_contents.get(), blink::WebInputEvent::kRawKeyDown);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents.get()));

  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents.get()));
}

TEST_F(DownloadRequestLimiterTest, SetHostContentSetting) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();
  SetHostContentSetting(web_contents(), CONTENT_SETTING_ALLOW);

  // The content setting will be checked first and overwrite the
  // ALLOW_ONE_DOWNLOAD default status.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  SetHostContentSetting(web_contents(), CONTENT_SETTING_BLOCK);

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, ContentSettingChanged) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Simulate an accidental deny.
  UpdateExpectations(CANCEL);
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set the content setting to allow and send the notification. Ensure that the
  // limiter states update to match.
  UpdateContentSettings(web_contents(), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask to download, and assert that it succeeded and we are still in allow.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set the content setting to block and send the notification. Ensure that the
  // limiter states updates to match.
  UpdateContentSettings(web_contents(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ensure downloads are blocked.
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Reset to ask. Verify that the download counts have not changed on the
  // content settings change (ensuring there is no "free" download after
  // changing the content setting).
  UpdateContentSettings(web_contents(), CONTENT_SETTING_ASK);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  UpdateExpectations(WAIT);
  CanDownload();
  ExpectAndResetCounts(0, 0, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

// Test that renderer initiated download from another origin are handled
// properly.
TEST_F(DownloadRequestLimiterTest, RendererInitiatedDownloadFromAnotherOrigin) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Sets the content setting to block for another origin.
  SetHostContentSetting(web_contents(), GURL("http://foobar.com"),
                        CONTENT_SETTING_BLOCK);

  // Trigger a renderer initiated download from the other origin.
  CanDownloadFor(web_contents(),
                 url::Origin::Create(GURL("http://foobar.com")));
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // The current tab is not affected, still allowing one download.
  CanDownloadFor(web_contents());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Change the content setting to allow for the other origin.
  SetHostContentSetting(web_contents(), GURL("http://foobar.com"),
                        CONTENT_SETTING_ALLOW);
  CanDownloadFor(web_contents(),
                 url::Origin::Create(GURL("http://foobar.com")));
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Trigger another download in the current tab, and cancel the prompt.
  UpdateExpectations(CANCEL);
  CanDownloadFor(web_contents());
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Download should proceed for the other origin.
  CanDownloadFor(web_contents(),
                 url::Origin::Create(GURL("http://foobar.com")));
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}

// Test that user interaction on the current page won't reset download status
// for another origin.
TEST_F(DownloadRequestLimiterTest,
       DownloadStatusForOtherOriginsNotResetOnUserInteraction) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Trigger a renderer initiated download from the other origin.
  CanDownloadFor(web_contents(),
                 url::Origin::Create(GURL("http://foobar.com")));
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // The current tab is not affected, still allowing one download.
  CanDownloadFor(web_contents());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // On user interaction, the current page should reset its download status.
  OnUserInteraction(blink::WebInputEvent::kTouchStart);
  CanDownloadFor(web_contents());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));

  // Download status from the other origin does not reset and should prompt.
  UpdateExpectations(CANCEL);
  CanDownloadFor(web_contents(),
                 url::Origin::Create(GURL("http://foobar.com")));
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED,
            download_request_limiter_->GetDownloadUiStatus(web_contents()));
}
