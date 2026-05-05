// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorBackendFactoryTestBase : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                              HistoryServiceFactory::GetDefaultFactory());
    profile_ = builder.Build();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

class AccessibilityAnnotatorBackendFactoryTest
    : public AccessibilityAnnotatorBackendFactoryTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAccessibilityAnnotator);
    AccessibilityAnnotatorBackendFactoryTestBase::SetUp();
  }
};

TEST_F(AccessibilityAnnotatorBackendFactoryTest,
       ServiceCreatedForRegularProfile) {
  EXPECT_NE(nullptr, AccessibilityAnnotatorBackendFactory::GetForProfile(
                         profile_.get()));
}

TEST_F(AccessibilityAnnotatorBackendFactoryTest,
       ServiceRedirectedForIncognitoProfile) {
  Profile* otr_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);

  EXPECT_EQ(AccessibilityAnnotatorBackendFactory::GetForProfile(profile_.get()),
            AccessibilityAnnotatorBackendFactory::GetForProfile(otr_profile));
}

class AccessibilityAnnotatorBackendFactoryDisabledTest
    : public AccessibilityAnnotatorBackendFactoryTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kAccessibilityAnnotator);
    AccessibilityAnnotatorBackendFactoryTestBase::SetUp();
  }
};

TEST_F(AccessibilityAnnotatorBackendFactoryDisabledTest, ServiceDisabled) {
  EXPECT_EQ(nullptr, AccessibilityAnnotatorBackendFactory::GetForProfile(
                         profile_.get()));
}

TEST_F(AccessibilityAnnotatorBackendFactoryDisabledTest,
       DeleteDatabaseWhenFeaturesDisabled) {
  base::FilePath db_path =
      profile_->GetPath().Append(FILE_PATH_LITERAL("AccessibilityAnnotatorDB"));

  ASSERT_TRUE(base::WriteFile(db_path, "dummy content"));
  ASSERT_TRUE(base::PathExists(db_path));

  // Trigger the deletion logic.
  AccessibilityAnnotatorBackendFactory::GetForProfile(profile_.get());

  // `task_environment_.RunUntilIdle()` is required here. The database deletion
  // logic is triggered asynchronously and executes on a background thread pool
  // thread. Crucially, this background task does not post a reply task back to
  // the main thread upon completion.
  //
  // If we used a utility like `base::test::RunUntil` to wait for the file to be
  // deleted, it would be flake-prone from a deadlock/timeout. `RunUntil`
  // evaluates the condition whenever the main thread becomes idle, and relies
  // on cross-thread signaling (e.g., a reply task) to wake up the main thread's
  // message pump to re-evaluate.
  //
  // `RunUntilIdle()` avoids this by forcing the task environment to run all
  // pending tasks across both the main thread and the ThreadPool until both
  // are empty, ensuring the background deletion completes before we proceed.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(db_path));
}

}  // namespace accessibility_annotator
