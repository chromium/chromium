// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_model_observer_helper.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class AutoPictureInPictureTabModelObserverHelperTest : public ::testing::Test {
 public:
  AutoPictureInPictureTabModelObserverHelperTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    tab_model_ = std::make_unique<OwningTestTabModel>(profile_.get());

    // Create the WebContents for the observed tab and pass its raw pointer to
    // the helper.
    std::unique_ptr<content::WebContents> owned_web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile_.get()));
    raw_ptr<content::WebContents> raw_web_contents = owned_web_contents.get();

    helper_ = std::make_unique<AutoPictureInPictureTabModelObserverHelper>(
        raw_web_contents, callback_.Get());
    helper_->StartObserving();

    // Add the observed tab to the TabModel, transferring ownership.
    // This triggers the activation callback synchronously.
    EXPECT_CALL(callback_, Run(true));
    observed_tab_ =
        tab_model_->AddTabFromWebContents(std::move(owned_web_contents),
                                          /*index=*/0, /*select=*/true);
    testing::Mock::VerifyAndClearExpectations(&callback_);
    ASSERT_TRUE(observed_tab_);
  }

  void TearDown() override {
    helper_.reset();
    tab_model_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<OwningTestTabModel> tab_model_;
  raw_ptr<TabAndroid> observed_tab_;
  base::MockCallback<
      AutoPictureInPictureTabObserverHelperBase::ActivatedChangedCallback>
      callback_;
  std::unique_ptr<AutoPictureInPictureTabModelObserverHelper> helper_;
};

TEST_F(AutoPictureInPictureTabModelObserverHelperTest, DebounceLogic) {
  // The observed tab is already added and selected in SetUp.

  // Select another tab within the debounce window (50ms).
  // The state transition to "inactive" should be ignored because it happens too
  // quickly after the last "self-select" event.
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_CALL(callback_, Run(_)).Times(0);
  tab_model_->AddEmptyTab(/*index=*/1, /*select=*/true);
  testing::Mock::VerifyAndClearExpectations(&callback_);

  // Re-select the observed tab.
  // We are still effectively "active" because the previous "inactive" signal
  // was debounced. Therefore, selecting the tab again (staying true) triggers
  // no callback.
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_CALL(callback_, Run(_)).Times(0);
  tab_model_->SetActiveIndex(0);
  testing::Mock::VerifyAndClearExpectations(&callback_);

  // Select another tab outside the debounce window.
  // Wait past the debounce threshold (100ms).
  task_environment_.FastForwardBy(base::Milliseconds(200));

  // Now the callback SHOULD fire with false (inactive) because enough time
  // has passed to consider this a stable state change.
  EXPECT_CALL(callback_, Run(false));
  tab_model_->SetActiveIndex(1);
  testing::Mock::VerifyAndClearExpectations(&callback_);
}
