// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_model_observer_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
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

  EXPECT_TRUE(helper_->IsTabActivated());
  helper_->StopObserving();
  EXPECT_TRUE(helper_->IsTabActivated());
  // A query while stopped must not cache the unobserved model.
  EXPECT_EQ(nullptr, helper_->GetActiveWebContents());
  tab_model_->SetActiveIndex(1);
  EXPECT_FALSE(helper_->IsTabActivated());
  // Reactivate the tab so that only the removal can make this false.
  tab_model_->SetActiveIndex(0);
  EXPECT_TRUE(helper_->IsTabActivated());
  TabModelList::RemoveTabModel(tab_model_.get());
  EXPECT_FALSE(helper_->IsTabActivated());
  TabModelList::AddTabModel(tab_model_.get());
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

TEST_F(AutoPictureInPictureTabModelObserverHelperTest,
       ProfileScopingAndStopObservingAfterProfileWillBeDestroyed) {
  auto web_contents = CreateTestWebContents();
  content::WebContents* raw_contents = web_contents.get();
  auto profile_delegate = std::make_unique<
      MockAutoPictureInPictureTabModelObserverHelperDelegate>();
  auto* raw_profile_delegate = profile_delegate.get();
  helper_ = std::make_unique<AutoPictureInPictureTabModelObserverHelper>(
      raw_contents, callback_.Get(), std::move(profile_delegate));

  // Start observing before `raw_contents` is in any TabModel, then add a
  // second TabModel for `profile()` and insert `web_contents` into it to verify
  // `OnTabModelAdded()` and `OnTabModelRemoved()`.
  EXPECT_CALL(callback_, Run(_)).Times(0);
  helper_->StartObserving();
  EXPECT_EQ(nullptr, helper_->GetActiveWebContents());

  OwningTestTabModel second_model(profile());
  EXPECT_CALL(*raw_profile_delegate, IsTabDragging(raw_contents))
      .WillOnce(Return(false));
  EXPECT_CALL(callback_, Run(true));
  second_model.AddTabFromWebContents(std::move(web_contents), 0, true);
  EXPECT_EQ(raw_contents, helper_->GetActiveWebContents());
  testing::Mock::VerifyAndClearExpectations(&callback_);
  testing::Mock::VerifyAndClearExpectations(raw_profile_delegate);

  // Removing `second_model` from `TabModelList` while observing clears
  // `observed_tab_model_` and stops observing `second_model`.
  TabModelList::RemoveTabModel(&second_model);
  EXPECT_EQ(nullptr, helper_->GetActiveWebContents());
  EXPECT_CALL(*raw_profile_delegate, IsTabDragging(_)).Times(0);
  EXPECT_CALL(callback_, Run(_)).Times(0);
  second_model.AddEmptyTab(1, true);
  TabModelList::AddTabModel(&second_model);
  helper_.reset();

  std::unique_ptr<TestingProfile> other = TestingProfile::Builder().Build();
  Profile* otr = profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  OwningTestTabModel other_before(other.get());
  other_before.AddEmptyTab(0, true);

  {
    OwningTestTabModel otr_model(otr);
    TabAndroid* otr_tab = otr_model.AddEmptyTab(0, true);
    auto delegate = std::make_unique<
        MockAutoPictureInPictureTabModelObserverHelperDelegate>();
    auto* raw_delegate = delegate.get();
    auto otr_helper =
        std::make_unique<AutoPictureInPictureTabModelObserverHelper>(
            otr_tab->web_contents(), callback_.Get(), std::move(delegate));

    EXPECT_CALL(callback_, Run(true));
    otr_helper->StartObserving();
    testing::Mock::VerifyAndClearExpectations(&callback_);

    EXPECT_CALL(*raw_delegate, IsTabDragging(_)).Times(0);
    EXPECT_CALL(callback_, Run(_)).Times(0);
    tab_model_->AddEmptyTab(0, true);
    other_before.AddEmptyTab(1, true);
    OwningTestTabModel other_after(other.get());
    other_after.AddEmptyTab(0, true);

    otr->MaybeSendDestroyedNotification();
    otr_helper->StartObserving();
    otr_model.AddEmptyTab(1, true);
    otr_helper->StopObserving();
    otr_helper.reset();
  }
  profile()->DestroyOffTheRecordProfile(otr);
}
