// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_resource_throttle.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/mock_download_controller.h"
#endif

namespace {

const char kTestUrl[] = "http://www.example.com/";

}  // namespace

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() {}
  ~MockWebContentsDelegate() override {}
};

class MockResourceThrottleDelegate
    : public content::ResourceThrottle::Delegate {
 public:
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(CancelAndIgnore, void());
  MOCK_METHOD1(CancelWithError, void(int));
  MOCK_METHOD0(Resume, void());
};

// Posts |quit_closure| to UI thread.
ACTION_P(QuitLoop, quit_closure) {
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           quit_closure);
}

class DownloadResourceThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  DownloadResourceThrottleTest()
      : ChromeRenderViewHostTestHarness(
            content::TestBrowserThreadBundle::REAL_IO_THREAD),
        throttle_(nullptr),
        limiter_(new DownloadRequestLimiter()) {}

  ~DownloadResourceThrottleTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents()->SetDelegate(&delegate_);
    run_loop_.reset(new base::RunLoop());
#if defined(OS_ANDROID)
    DownloadControllerBase::SetDownloadControllerBase(&download_controller_);
#endif
  }

  void TearDown() override {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       throttle_);
#if defined(OS_ANDROID)
    DownloadControllerBase::SetDownloadControllerBase(nullptr);
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void StartThrottleOnIOThread(int process_id, int render_view_id) {
    throttle_ = new DownloadResourceThrottle(
        limiter_,
        base::Bind(&tab_util::GetWebContentsByID, process_id, render_view_id),
        GURL(kTestUrl), "GET");
    throttle_->set_delegate_for_testing(&resource_throttle_delegate_);
    bool defer;
    throttle_->WillStartRequest(&defer);
    EXPECT_EQ(true, defer);
  }

  void StartThrottle() {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &DownloadResourceThrottleTest::StartThrottleOnIOThread,
            base::Unretained(this),
            web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
            web_contents()->GetRenderViewHost()->GetRoutingID()));
    run_loop_->Run();
  }

 protected:
  content::ResourceThrottle* throttle_;
  MockWebContentsDelegate delegate_;
  scoped_refptr<DownloadRequestLimiter> limiter_;
  ::testing::NiceMock<MockResourceThrottleDelegate> resource_throttle_delegate_;
  std::unique_ptr<base::RunLoop> run_loop_;
#if defined(OS_ANDROID)
  chrome::android::MockDownloadController download_controller_;
#endif
};

TEST_F(DownloadResourceThrottleTest, StartDownloadThrottle_Basic) {
  EXPECT_CALL(resource_throttle_delegate_, Resume())
      .WillOnce(QuitLoop(run_loop_->QuitClosure()));
  StartThrottle();
}

#if defined(OS_ANDROID)
TEST_F(DownloadResourceThrottleTest, DownloadWithFailedFileAcecssRequest) {
  DownloadControllerBase::Get()
      ->SetApproveFileAccessRequestForTesting(false);
  EXPECT_CALL(resource_throttle_delegate_, Cancel())
      .WillOnce(QuitLoop(run_loop_->QuitClosure()));
  StartThrottle();
}
#endif
