// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/browser_closer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace enterprise_idle {

using CloseResult = BrowserCloser::CloseResult;
using base::test::RunClosure;

class BrowserCloserTest : public BrowserWithTestWindowTest {
 public:
  BrowserCloserTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    test_views_delegate()->set_use_desktop_native_widgets(true);
    BrowserWithTestWindowTest::SetUp();
  }
};

TEST_F(BrowserCloserTest, Basic) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(CloseResult)> callback;
  EXPECT_CALL(callback, Run(CloseResult::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  auto subscription = BrowserCloser::GetInstance()->ShowDialogAndCloseBrowsers(
      profile(), base::Minutes(5), callback.Get());
  task_environment()->FastForwardBy(base::Seconds(30));
  run_loop.Run();
}

TEST_F(BrowserCloserTest, DismissedByUser) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(CloseResult)> callback;
  EXPECT_CALL(callback, Run(CloseResult::kAborted))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  auto subscription = BrowserCloser::GetInstance()->ShowDialogAndCloseBrowsers(
      profile(), base::Minutes(5), callback.Get());
  BrowserCloser::GetInstance()->DismissDialogForTesting();
  run_loop.Run();
}

TEST_F(BrowserCloserTest, ProfileHasNoBrowsers) {
  set_browser(nullptr);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(CloseResult)> callback;
  EXPECT_CALL(callback, Run(CloseResult::kSkip))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  auto subscription = BrowserCloser::GetInstance()->ShowDialogAndCloseBrowsers(
      profile(), base::Minutes(5), callback.Get());
  run_loop.Run();
}

}  // namespace enterprise_idle
