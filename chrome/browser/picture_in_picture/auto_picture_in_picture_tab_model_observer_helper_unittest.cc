// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_model_observer_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

class MockAutoPictureInPictureTabModelObserverHelperDelegate
    : public AutoPictureInPictureTabModelObserverHelper::Delegate {
 public:
  MOCK_CONST_METHOD1(IsTabDragging, bool(content::WebContents*));
};

class AutoPictureInPictureTabModelObserverHelperTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_model_ = std::make_unique<OwningTestTabModel>(profile());
  }

  void TearDown() override {
    helper_.reset();
    tab_model_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<OwningTestTabModel> tab_model_;
  base::MockCallback<
      AutoPictureInPictureTabObserverHelperBase::ActivatedChangedCallback>
      callback_;
  std::unique_ptr<AutoPictureInPictureTabModelObserverHelper> helper_;
};

TEST_F(AutoPictureInPictureTabModelObserverHelperTest, UpdatesActivationState) {
  auto web_contents = CreateTestWebContents();
  content::WebContents* raw_contents = web_contents.get();
  tab_model_->AddTabFromWebContents(std::move(web_contents), 0, true);

  auto delegate = std::make_unique<
      MockAutoPictureInPictureTabModelObserverHelperDelegate>();
  MockAutoPictureInPictureTabModelObserverHelperDelegate* raw_delegate =
      delegate.get();

  helper_ = std::make_unique<AutoPictureInPictureTabModelObserverHelper>(
      raw_contents, callback_.Get(), std::move(delegate));

  EXPECT_CALL(callback_, Run(true));
  helper_->StartObserving();

  EXPECT_CALL(*raw_delegate, IsTabDragging(raw_contents))
      .WillOnce(Return(false));
  EXPECT_CALL(callback_, Run(false));
  tab_model_->AddEmptyTab(1, true);

  EXPECT_CALL(*raw_delegate, IsTabDragging(raw_contents))
      .WillOnce(Return(false));
  EXPECT_CALL(callback_, Run(true));
  tab_model_->SetActiveIndex(0);
}

TEST_F(AutoPictureInPictureTabModelObserverHelperTest,
       IgnoresActivationChangeWhileDragging) {
  auto web_contents = CreateTestWebContents();
  content::WebContents* raw_contents = web_contents.get();
  tab_model_->AddTabFromWebContents(std::move(web_contents), 0, true);

  auto delegate = std::make_unique<
      MockAutoPictureInPictureTabModelObserverHelperDelegate>();
  MockAutoPictureInPictureTabModelObserverHelperDelegate* raw_delegate =
      delegate.get();

  helper_ = std::make_unique<AutoPictureInPictureTabModelObserverHelper>(
      raw_contents, callback_.Get(), std::move(delegate));

  EXPECT_CALL(callback_, Run(true));
  helper_->StartObserving();

  // Suppress activation change while dragging.
  EXPECT_CALL(*raw_delegate, IsTabDragging(raw_contents))
      .WillOnce(Return(true));
  EXPECT_CALL(callback_, Run(_)).Times(0);
  tab_model_->AddEmptyTab(1, true);
  testing::Mock::VerifyAndClearExpectations(&callback_);
  testing::Mock::VerifyAndClearExpectations(raw_delegate);

  // Stop dragging and verify it now updates on selection.
  EXPECT_CALL(*raw_delegate, IsTabDragging(raw_contents))
      .WillOnce(Return(false));
  EXPECT_CALL(callback_, Run(false));
  tab_model_->AddEmptyTab(2, true);
}
