// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/survey/arc_survey_service.h"
#include <cstddef>
#include <cstdint>
#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
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
    EXPECT_EQ(2u, arc_survey_service_->GetAllowedPackagesForTesting()->size());
  }

  void TearDown() override {
    arc_service_manager_.set_browser_context(nullptr);
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

  bool LoadSurveyData(std::string package_names) {
    return arc_survey_service_->LoadSurveyData(package_names);
  }

  std::set<std::string>& GetAllowedPackageNameSet() {
    return arc_survey_service_->allowed_packages_;
  }

  const base::TimeDelta GetElapsedTimeSurveyTrigger() {
    return arc_survey_service_->elapsed_time_survey_trigger_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  ArcServiceManager arc_service_manager_;
  raw_ptr<ArcSurveyService> arc_survey_service_ = nullptr;
};

TEST_F(ArcSurveyServiceTest, ConstructDestruct) {}

TEST_F(ArcSurveyServiceTest, SingleTask) {
  // Create task
  EXPECT_TRUE(GetPackageNameMap()->empty());
  OnTaskCreated(1, kPackageA);

  EXPECT_EQ(1u, GetPackageNameMap()->size());
  EXPECT_EQ(kPackageA, GetPackageNameMap()->find(kPackageA)->first);
  EXPECT_EQ(1, GetPackageNameMap()->find(kPackageA)->second.first);
  EXPECT_EQ(1u, getTaskIdMap()->size());
  EXPECT_EQ(1, getTaskIdMap()->find(1)->first);
  EXPECT_EQ(kPackageA, getTaskIdMap()->find(1)->second);

  // Destroy unrelated task
  OnTaskDestroyed(10);
  EXPECT_EQ(1u, GetPackageNameMap()->size());
  EXPECT_EQ(1u, getTaskIdMap()->size());

  // Destroy original task
  OnTaskDestroyed(1);
  EXPECT_TRUE(GetPackageNameMap()->empty());
  EXPECT_TRUE(getTaskIdMap()->empty());
}

TEST_F(ArcSurveyServiceTest, MultiPackage) {
  // Create 2 tasks
  EXPECT_TRUE(GetPackageNameMap()->empty());
  OnTaskCreated(1, kPackageA);
  OnTaskCreated(2, kPackageB);

  EXPECT_EQ(2u, GetPackageNameMap()->size());  // Verify 2 entries
  EXPECT_EQ(kPackageA, GetPackageNameMap()->find(kPackageA)->first);
  EXPECT_EQ(1, GetPackageNameMap()->find(kPackageA)->second.first);
  EXPECT_EQ(kPackageB, GetPackageNameMap()->find(kPackageB)->first);
  EXPECT_EQ(1, GetPackageNameMap()->find(kPackageB)->second.first);
  EXPECT_EQ(2u, getTaskIdMap()->size());  // Verify 2 entries
  EXPECT_EQ(1, getTaskIdMap()->find(1)->first);
  EXPECT_EQ(kPackageA, getTaskIdMap()->find(1)->second);
  EXPECT_EQ(2, getTaskIdMap()->find(2)->first);
  EXPECT_EQ(kPackageB, getTaskIdMap()->find(2)->second);

  // Destroy task w/ ID 2
  OnTaskDestroyed(2);
  EXPECT_EQ(1u, GetPackageNameMap()->size());
  EXPECT_EQ(1u, getTaskIdMap()->size());

  // Destroy task w/ ID 1
  OnTaskDestroyed(1);
  EXPECT_TRUE(GetPackageNameMap()->empty());
  EXPECT_TRUE(getTaskIdMap()->empty());
}

TEST_F(ArcSurveyServiceTest, MultiTask) {
  // Create 2 tasks for the same package
  EXPECT_TRUE(GetPackageNameMap()->empty());
  OnTaskCreated(1, kPackageA);
  OnTaskCreated(2, kPackageA);

  EXPECT_EQ(1u, GetPackageNameMap()->size());
  EXPECT_EQ(kPackageA, GetPackageNameMap()->find(kPackageA)->first);
  EXPECT_EQ(2, GetPackageNameMap()->find(kPackageA)->second.first);
  EXPECT_EQ(2u, getTaskIdMap()->size());
  EXPECT_EQ(1, getTaskIdMap()->find(1)->first);
  EXPECT_EQ(kPackageA, getTaskIdMap()->find(1)->second);
  EXPECT_EQ(2, getTaskIdMap()->find(2)->first);
  EXPECT_EQ(kPackageA, getTaskIdMap()->find(2)->second);

  // Destroy task w/ ID 2
  OnTaskDestroyed(2);
  EXPECT_EQ(1u, GetPackageNameMap()->size());
  EXPECT_EQ(1u, getTaskIdMap()->size());

  // Destroy task w/ ID 1
  OnTaskDestroyed(1);
  EXPECT_TRUE(GetPackageNameMap()->empty());
  EXPECT_TRUE(getTaskIdMap()->empty());
}

TEST_F(ArcSurveyServiceTest, LoadSurveyData_InvalidFormats) {
  // Clear any existing entries.
  GetAllowedPackageNameSet().clear();
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());

  // No JSON String
  ASSERT_FALSE(LoadSurveyData("foobar1234"));
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());

  // |package_names| as a key and a string as its value, AND
  // |elapsed_time_survey_trigger| as a key and a string as its value.
  ASSERT_FALSE(LoadSurveyData(R"({
      "package_names":"com.android.vending",
      "elapsed_time_survey_trigger_ min":"foo"})"));
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());

  // |elapsed_time_survey_trigger| as the key and a string as its value.
  ASSERT_FALSE(LoadSurveyData(R"({
      "package_names":"com.android.vending",
      "elapsed_time_survey_trigger_ min":"foo")"));
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());

  // |package_names| as the key and not all the items in the list are strings.
  ASSERT_FALSE(LoadSurveyData(R"({
      "package_names":["com.android.vending",123]})"));
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
}

TEST_F(ArcSurveyServiceTest, LoadSurveyData_ValidFormat) {
  // Clear any existing entries.
  GetAllowedPackageNameSet().clear();
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());

  ASSERT_TRUE(LoadSurveyData(R"({
      "package_names":["com.android.vending","com.android.settings"],
      "elapsed_time_survey_trigger_min":200})"));
  EXPECT_EQ(2u, GetAllowedPackageNameSet().size());
  EXPECT_EQ(1u, GetAllowedPackageNameSet().count("com.android.vending"));
  EXPECT_EQ(1u, GetAllowedPackageNameSet().count("com.android.settings"));
  EXPECT_EQ(base::Minutes(200), GetElapsedTimeSurveyTrigger());
}

TEST_F(ArcSurveyServiceTest, LoadSurveyData_ValidFormat_EmptyList) {
  // Clear any existing entries.
  GetAllowedPackageNameSet().clear();
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());

  ASSERT_FALSE(LoadSurveyData(R"({"package_names":[]})"));
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());
}

TEST_F(ArcSurveyServiceTest, LoadSurveyData_ValidFormat_NoSurveyTrigger) {
  // Clear any existing entries.
  GetAllowedPackageNameSet().clear();
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());

  // No |elapsed_time_survey_trigger_min| key
  ASSERT_TRUE(LoadSurveyData(R"({
      "package_names":["com.android.vending","com.android.settings"]})"));
  EXPECT_EQ(2u, GetAllowedPackageNameSet().size());
  EXPECT_EQ(1u, GetAllowedPackageNameSet().count("com.android.vending"));
  EXPECT_EQ(1u, GetAllowedPackageNameSet().count("com.android.settings"));
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());
}

TEST_F(ArcSurveyServiceTest, LoadSurveyData_ValidFormat_CharSubstitution) {
  // Clear any existing entries.
  GetAllowedPackageNameSet().clear();
  EXPECT_TRUE(GetAllowedPackageNameSet().empty());
  EXPECT_EQ(base::Minutes(10), GetElapsedTimeSurveyTrigger());

  // Survey Data that requires character substitution.
  // \{@} --> :
  // \{~} --> ,
  // \{%} --> .
  ASSERT_TRUE(LoadSurveyData(R"({
      "package_names"\{@}[
         "com\{%}android\{%}vending"\{~}"com\{%}android\{%}settings"]\{~}
      "elapsed_time_survey_trigger_min"\{@}500})"));

  EXPECT_EQ(2u, GetAllowedPackageNameSet().size());
  EXPECT_EQ(1u, GetAllowedPackageNameSet().count("com.android.vending"));
  EXPECT_EQ(1u, GetAllowedPackageNameSet().count("com.android.settings"));
  EXPECT_EQ(base::Minutes(500), GetElapsedTimeSurveyTrigger());
}

}  // namespace arc
