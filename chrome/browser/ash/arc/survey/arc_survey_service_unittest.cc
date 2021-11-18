// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/survey/arc_survey_service.h"
#include <cstddef>
#include <cstdint>
#include <memory>

#include "chrome/test/base/testing_profile.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/arc_service_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kPackageA[] = "package A";
constexpr char kPackageB[] = "package B";

}  // namespace

class ArcSurveyServiceTest : public testing::Test {
 public:
  ArcSurveyServiceTest() = default;
  ~ArcSurveyServiceTest() override = default;

  void SetUp() override {
    // Arc services
    arc_service_manager_.set_browser_context(&testing_profile_);

    arc_survey_service_ =
        ArcSurveyService::GetForBrowserContextForTesting(&testing_profile_);
    arc_survey_service_->AddAllowedPackageNameForTesting(kPackageA);
    arc_survey_service_->AddAllowedPackageNameForTesting(kPackageB);
    EXPECT_EQ(2, arc_survey_service_->GetAllowedPackagesForTesting()->size());
  }

  void OnTaskCreated(int32_t task_id, const std::string package_name) {
    arc_survey_service_->OnTaskCreated(task_id, package_name, "" /* activity */,
                                       "" /* intent */, 0 /* session_id */);
  }

  void OnTaskDestroyed(int32_t task_id) {
    arc_survey_service_->OnTaskDestroyed(task_id);
  }

  const ArcSurveyService::PackageNameMap* GetPackageNameMap() {
    return arc_survey_service_->GetPackageNameMapForTesting();
  }

  const ArcSurveyService::TaskIdMap* getTaskIdMap() {
    return arc_survey_service_->GetTaskIdMapForTesting();
  }

  bool LoadPackageNames(const std::string package_names) {
    return arc_survey_service_->LoadPackageNames(package_names);
  }

  std::set<std::string>& GetAllowedPackageNameSet() {
    return arc_survey_service_->allowed_packages_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  ArcServiceManager arc_service_manager_;
  ArcSurveyService* arc_survey_service_ = nullptr;
};

TEST_F(ArcSurveyServiceTest, ConstructDestruct) {}

TEST_F(ArcSurveyServiceTest, SingleTask) {
  // Create task
  EXPECT_EQ(0, GetPackageNameMap()->size());
  OnTaskCreated(1, kPackageA);

  EXPECT_EQ(1, GetPackageNameMap()->size());
  EXPECT_EQ(kPackageA, GetPackageNameMap()->find(kPackageA)->first);
  EXPECT_EQ(1, GetPackageNameMap()->find(kPackageA)->second.first);
  EXPECT_EQ(1, getTaskIdMap()->size());
  EXPECT_EQ(1, getTaskIdMap()->find(1)->first);
  EXPECT_EQ(kPackageA, getTaskIdMap()->find(1)->second);

  // Destroy unrelated task
  OnTaskDestroyed(10);
  EXPECT_EQ(1, GetPackageNameMap()->size());
  EXPECT_EQ(1, getTaskIdMap()->size());

  // Destroy original task
  OnTaskDestroyed(1);
  EXPECT_EQ(0, GetPackageNameMap()->size());
  EXPECT_EQ(0, getTaskIdMap()->size());
}

TEST_F(ArcSurveyServiceTest, MultiPackage) {
  // Create 2 tasks
  EXPECT_EQ(0, GetPackageNameMap()->size());
  OnTaskCreated(1, kPackageA);
  OnTaskCreated(2, kPackageB);

  EXPECT_EQ(2, GetPackageNameMap()->size());  // Verify 2 entries
  EXPECT_EQ(kPackageA, GetPackageNameMap()->find(kPackageA)->first);
  EXPECT_EQ(1, GetPackageNameMap()->find(kPackageA)->second.first);
  EXPECT_EQ(kPackageB, GetPackageNameMap()->find(kPackageB)->first);
  EXPECT_EQ(1, GetPackageNameMap()->find(kPackageB)->second.first);
  EXPECT_EQ(2, getTaskIdMap()->size());  // Verify 2 entries
  EXPECT_EQ(1, getTaskIdMap()->find(1)->first);
  EXPECT_EQ(kPackageA, getTaskIdMap()->find(1)->second);
  EXPECT_EQ(2, getTaskIdMap()->find(2)->first);
  EXPECT_EQ(kPackageB, getTaskIdMap()->find(2)->second);

  // Destroy task w/ ID 2
  OnTaskDestroyed(2);
  EXPECT_EQ(1, GetPackageNameMap()->size());
  EXPECT_EQ(1, getTaskIdMap()->size());

  // Destroy task w/ ID 1
  OnTaskDestroyed(1);
  EXPECT_EQ(0, GetPackageNameMap()->size());
  EXPECT_EQ(0, getTaskIdMap()->size());
}

TEST_F(ArcSurveyServiceTest, MultiTask) {
  // Create 2 tasks for the same package
  EXPECT_EQ(0, GetPackageNameMap()->size());
  OnTaskCreated(1, kPackageA);
  OnTaskCreated(2, kPackageA);

  EXPECT_EQ(1, GetPackageNameMap()->size());
  EXPECT_EQ(kPackageA, GetPackageNameMap()->find(kPackageA)->first);
  EXPECT_EQ(2, GetPackageNameMap()->find(kPackageA)->second.first);
  EXPECT_EQ(2, getTaskIdMap()->size());
  EXPECT_EQ(1, getTaskIdMap()->find(1)->first);
  EXPECT_EQ(kPackageA, getTaskIdMap()->find(1)->second);
  EXPECT_EQ(2, getTaskIdMap()->find(2)->first);
  EXPECT_EQ(kPackageA, getTaskIdMap()->find(2)->second);

  // Destroy task w/ ID 2
  OnTaskDestroyed(2);
  EXPECT_EQ(1, GetPackageNameMap()->size());
  EXPECT_EQ(1, getTaskIdMap()->size());

  // Destroy task w/ ID 1
  OnTaskDestroyed(1);
  EXPECT_EQ(0, GetPackageNameMap()->size());
  EXPECT_EQ(0, getTaskIdMap()->size());
}

TEST_F(ArcSurveyServiceTest, LoadPackageNames) {
  // Clear any existing entries.
  GetAllowedPackageNameSet().clear();
  EXPECT_EQ(0, GetAllowedPackageNameSet().size());

  // INVALID: No JSON String
  EXPECT_FALSE(LoadPackageNames("foobar1234"));
  EXPECT_EQ(0, GetAllowedPackageNameSet().size());

  // Invalid JSON String: |package_names| as the key and a string as the value.
  EXPECT_FALSE(LoadPackageNames(R"({"package_names":"com.android.vending")"));
  EXPECT_EQ(0, GetAllowedPackageNameSet().size());

  // Invalid JSON format: |package_names| as the key and not all the items in
  // the list are strings.
  EXPECT_FALSE(
      LoadPackageNames(R"({"package_names":["com.android.vending",123]})"));
  EXPECT_EQ(0, GetAllowedPackageNameSet().size());

  // Valid JSON Format: |package_names| as the key and ALL the items in the list
  // are strings.
  EXPECT_TRUE(LoadPackageNames(
      R"({"package_names":["com.android.vending","com.android.settings"]})"));
  EXPECT_EQ(2, GetAllowedPackageNameSet().size());
  EXPECT_EQ(1, GetAllowedPackageNameSet().count("com.android.vending"));
  EXPECT_EQ(1, GetAllowedPackageNameSet().count("com.android.settings"));
}

}  // namespace arc
