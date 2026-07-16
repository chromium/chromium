// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/copy_warning_delegate_tracker.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class CopyWarningDelegateTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  CopyWarningDelegateTrackerTest() = default;
  ~CopyWarningDelegateTrackerTest() override = default;

  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  test::FakeContentAnalysisDelegate* CreateDelegate(
      base::RepeatingClosure delete_closure = base::DoNothing(),
      ContentAnalysisDelegate::CompletionCallback callback =
          base::DoNothing()) {
    ContentAnalysisDelegate::Data data;
    return new test::FakeContentAnalysisDelegate(
        delete_closure,
        base::BindRepeating([](const std::string&, const base::FilePath&) {
          return test::FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"});
        }),
        "dm_token", web_contents(), std::move(data), std::move(callback),
        DeepScanAccessPoint::COPY);
  }
};

TEST_F(CopyWarningDelegateTrackerTest, SetAndClearIfMatches) {
  bool main_delegate_deleted = false;
  auto main_delegate = base::WrapUnique(CreateDelegate(
      base::BindLambdaForTesting([&]() { main_delegate_deleted = true; })));
  CopyWarningDelegateTracker::SetDelegate(web_contents(), main_delegate.get());

  auto* tracker = CopyWarningDelegateTracker::FromWebContents(web_contents());
  ASSERT_TRUE(tracker);

  // Clearing with a different delegate should be a no-op, leaving the main
  // delegate intact and undeleted.
  bool different_delegate_deleted = false;
  auto different_delegate =
      base::WrapUnique(CreateDelegate(base::BindLambdaForTesting(
          [&]() { different_delegate_deleted = true; })));

  CopyWarningDelegateTracker::ClearIfMatches(web_contents(),
                                             different_delegate.get());

  // The main delegate should NOT have been affected by the ClearIfMatches call.
  EXPECT_FALSE(main_delegate_deleted);

  // Now clear with the correct delegate.
  CopyWarningDelegateTracker::ClearIfMatches(web_contents(),
                                             main_delegate.get());

  // The delegate shouldn't be deleted by ClearIfMatches.
  EXPECT_FALSE(main_delegate_deleted);
}

TEST_F(CopyWarningDelegateTrackerTest, SetDelegate_ReplacesOld) {
  // First delegate would be deleted by SetDelegate and cannot be unique_ptr.
  auto* first_delegate = CreateDelegate();
  CopyWarningDelegateTracker::SetDelegate(web_contents(), first_delegate);

  auto second_delegate = base::WrapUnique(CreateDelegate());

  // This should delete the first_delegate automatically since we are setting a
  // new delegate.
  CopyWarningDelegateTracker::SetDelegate(web_contents(),
                                          second_delegate.get());

  // Clear second so tracker drops it, then delete to avoid leak.
  CopyWarningDelegateTracker::ClearIfMatches(web_contents(),
                                             second_delegate.get());
}

TEST_F(CopyWarningDelegateTrackerTest, SetDelegate_SameDelegate) {
  bool delegate_deleted = false;
  auto delegate = base::WrapUnique(CreateDelegate(
      base::BindLambdaForTesting([&]() { delegate_deleted = true; })));
  CopyWarningDelegateTracker::SetDelegate(web_contents(), delegate.get());

  // Calling it again with the same delegate should just keep it.
  CopyWarningDelegateTracker::SetDelegate(web_contents(), delegate.get());
  EXPECT_FALSE(delegate_deleted);

  CopyWarningDelegateTracker::ClearIfMatches(web_contents(), delegate.get());
}

TEST_F(CopyWarningDelegateTrackerTest, BypassAndClear) {
  bool delegate_deleted = false;
  bool callback_ran = false;

  auto* delegate = CreateDelegate(
      base::BindLambdaForTesting([&]() { delegate_deleted = true; }),
      base::BindLambdaForTesting([&](const ContentAnalysisDelegate::Data& data,
                                     ContentAnalysisDelegate::Result& result) {
        callback_ran = true;
      }));

  CopyWarningDelegateTracker::SetDelegate(web_contents(), delegate);

  // This should call BypassWarnings, invoke the completion callback, and delete
  // the delegate.
  CopyWarningDelegateTracker::BypassAndClear(web_contents());

  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(delegate_deleted);
}

}  // namespace enterprise_connectors
