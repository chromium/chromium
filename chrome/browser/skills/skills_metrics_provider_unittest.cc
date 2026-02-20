// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_metrics_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/skills/features.h"
#include "components/skills/mocks/mock_skills_service.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace skills {

class SkillsMetricsProviderTest : public testing::Test {
 public:
  SkillsMetricsProviderTest() {
    feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  void TearDown() override {
    profile_manager_.reset();
    mock_data_storage_.clear();
  }

  std::vector<Profile*> GetTestProfiles() {
    if (!profile_manager_) {
      return {};
    }
    return profile_manager_->profile_manager()->GetLoadedProfiles();
  }

  static std::unique_ptr<KeyedService> BuildMockSkillsService(
      content::BrowserContext* context) {
    return std::make_unique<NiceMock<MockSkillsService>>();
  }

  // Creates a testing profile and configures its SkillsService to return
  // a specific number of mock skills.
  void AddProfileWithSkills(const std::string& profile_name, int count) {
    TestingProfile::TestingFactories factories;
    factories.emplace_back(SkillsServiceFactory::GetInstance(),
                           base::BindRepeating(&BuildMockSkillsService));
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        profile_name, std::move(factories));
    auto* mock_service = static_cast<MockSkillsService*>(
        SkillsServiceFactory::GetForProfile(profile));
    CHECK(mock_service);
    // Prepare the test data.
    auto skills_vector =
        std::make_unique<std::vector<std::unique_ptr<Skill>>>();
    for (int i = 0; i < count; ++i) {
      auto skill = std::make_unique<Skill>();
      skill->id = "id" + base::NumberToString(i);
      skill->name = "name";
      skills_vector->push_back(std::move(skill));
    }
    // Transfer ownership of the data to the test fixture.
    mock_data_storage_.push_back(std::move(skills_vector));
    const auto& persistent_skills = *mock_data_storage_.back();
    // Return the data when the metrics provider asks for it.
    EXPECT_CALL(*mock_service, GetSkills())
        .WillRepeatedly(ReturnRef(persistent_skills));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Inject the callback into the provider
  SkillsMetricsProvider metrics_provider_{
      base::BindRepeating(&SkillsMetricsProviderTest::GetTestProfiles,
                          base::Unretained(this))};
  base::test::ScopedFeatureList feature_list_;

  // Owns the vectors of skills used by the mocks.
  // This is a vector of vectors because we may have multiple profiles,
  // each needing its own independent list of skills.
  std::vector<std::unique_ptr<std::vector<std::unique_ptr<Skill>>>>
      mock_data_storage_;

  // A placeholder skill object to return from AddSkill() expectations.
  // This avoids overhead and potential static destruction issues.
  Skill dummy_skill_;
};

namespace {

TEST_F(SkillsMetricsProviderTest, NoProfiles) {
  base::HistogramTester histogram_tester;
  metrics_provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester.ExpectTotalCount("Skills.UserSkills.Count", 0);
}

TEST_F(SkillsMetricsProviderTest, SingleProfile) {
  AddProfileWithSkills("hello@gmail.com", 5);
  base::HistogramTester histogram_tester;
  metrics_provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester.ExpectUniqueSample("Skills.UserSkills.Count", 5, 1);
}

TEST_F(SkillsMetricsProviderTest, MultiProfile_ReportsForEach) {
  AddProfileWithSkills("personal@gmail.com", 2);
  AddProfileWithSkills("work@google.com", 10);
  AddProfileWithSkills("test@example.com", 0);
  base::HistogramTester histogram_tester;

  // Trigger the provider
  metrics_provider_.ProvideCurrentSessionData(nullptr);

  // Expect 3 total samples recorded.
  histogram_tester.ExpectTotalCount("Skills.UserSkills.Count", 3);
  // Expect exactly one sample in each of these specific buckets.
  histogram_tester.ExpectBucketCount("Skills.UserSkills.Count", 2, 1);
  histogram_tester.ExpectBucketCount("Skills.UserSkills.Count", 10, 1);
  histogram_tester.ExpectBucketCount("Skills.UserSkills.Count", 0, 1);
}

}  // namespace
}  // namespace skills
