// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_pref_change_handler.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingPrefChangeHandlerAndroidTest : public testing::Test {
 public:
  SafeBrowsingPrefChangeHandlerAndroidTest() = default;

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    // Pass the profile to the constructor.
    pref_change_handler_ =
        std::make_unique<SafeBrowsingPrefChangeHandler>(profile_.get());
    rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
  }

  void TearDown() override { pref_change_handler_.reset(); }

  TestingProfile* profile() { return profile_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SafeBrowsingPrefChangeHandler> pref_change_handler_;
};

TEST_F(SafeBrowsingPrefChangeHandlerAndroidTest,
       AddAndRemoveTabModelListObserver) {
  EXPECT_FALSE(pref_change_handler_->IsObservingTabModelListForTesting());

  pref_change_handler_->AddTabModelListObserver();
  EXPECT_TRUE(pref_change_handler_->IsObservingTabModelListForTesting());

  pref_change_handler_->RemoveTabModelListObserver();
  EXPECT_FALSE(pref_change_handler_->IsObservingTabModelListForTesting());
}

TEST_F(SafeBrowsingPrefChangeHandlerAndroidTest, AddAndRemoveTabModelObserver) {
  // Create a test tab model.
  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);

  EXPECT_FALSE(pref_change_handler_->IsObservingTabModelForTesting());
  // Set tab model for testing after initial check
  pref_change_handler_->SetTabModelForTesting(&tab_model);
  pref_change_handler_->AddTabModelObserver();
  EXPECT_TRUE(pref_change_handler_->IsObservingTabModelForTesting());

  pref_change_handler_->RemoveTabModelObserver();
  EXPECT_FALSE(pref_change_handler_->IsObservingTabModelForTesting());

  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(SafeBrowsingPrefChangeHandlerAndroidTest,
       AddTabModelObserver_NoMatchingProfile) {
  // Create a different profile.
  TestingProfile::Builder other_profile_builder;
  std::unique_ptr<TestingProfile> other_profile = other_profile_builder.Build();
  TestTabModel tab_model(other_profile.get());
  TabModelList::AddTabModel(&tab_model);

  pref_change_handler_->AddTabModelObserver();
  EXPECT_FALSE(
      pref_change_handler_
          ->IsObservingTabModelForTesting());  // Should not be observing any
                                               // tab model.
  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(SafeBrowsingPrefChangeHandlerAndroidTest, RegisterObserver) {
  // Create a test tab model.
  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);

  pref_change_handler_->RegisterObserver();
  EXPECT_TRUE(pref_change_handler_->IsObservingTabModelListForTesting());
  EXPECT_TRUE(pref_change_handler_->IsObservingTabModelForTesting());

  pref_change_handler_->RemoveTabModelObserver();
  pref_change_handler_->RemoveTabModelListObserver();
  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(SafeBrowsingPrefChangeHandlerAndroidTest, DidAddTab) {
  // Create a test tab model and web contents.
  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  // Use the TestTabModel to create and manage the TabAndroid.
  tab_model.SetWebContentsList({web_contents.get()});
  TabAndroid* tab = tab_model.GetTabAt(0);

  pref_change_handler_->AddTabModelObserver();
  pref_change_handler_->AddTabModelListObserver();
  // Simulate adding a tab.
  pref_change_handler_->DidAddTab(tab, TabModel::TabLaunchType::FROM_LINK);

  // After adding a tab and calling DidAddTab, the observers should be removed.
  EXPECT_FALSE(pref_change_handler_->IsObservingTabModelForTesting());
  EXPECT_FALSE(pref_change_handler_->IsObservingTabModelListForTesting());
  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(SafeBrowsingPrefChangeHandlerAndroidTest, OnTabModelAddedAndRemoved) {
  TestTabModel tab_model(profile());
  pref_change_handler_->AddTabModelListObserver();
  // TabModel is not added yet, this should not do anything.
  pref_change_handler_->OnTabModelAdded(nullptr);
  EXPECT_FALSE(pref_change_handler_->IsObservingTabModelForTesting());

  // Set tab model for test
  pref_change_handler_->SetTabModelForTesting(&tab_model);
  TabModelList::AddTabModel(&tab_model);
  EXPECT_TRUE(pref_change_handler_->IsObservingTabModelForTesting());

  TabModelList::RemoveTabModel(&tab_model);
  EXPECT_FALSE(pref_change_handler_->IsObservingTabModelForTesting());
}

}  // namespace safe_browsing
