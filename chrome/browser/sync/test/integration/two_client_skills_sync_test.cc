// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SkillsMatchChecker : public StatusChangeChecker,
                           public skills::SkillsService::Observer {
 public:
  explicit SkillsMatchChecker(
      const std::vector<skills::SkillsService*>& services)
      : services_(services) {
    for (skills::SkillsService* service : services_) {
      CHECK(service);
      auto observation = std::make_unique<base::ScopedObservation<
          skills::SkillsService, skills::SkillsService::Observer>>(this);
      observation->Observe(service);
      observations_.push_back(std::move(observation));
    }
  }

  // skills::SkillsService::Observer overrides.
  void OnSkillUpdated(std::string_view skill_id,
                      skills::SkillsService::UpdateSource update_source,
                      bool is_position_changed) override {
    CheckExitCondition();
  }

  // StatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for skills to match across all clients. ";

    if (services_.empty()) {
      return true;
    }

    const std::vector<std::unique_ptr<skills::Skill>>& base_skills =
        services_.front()->GetSkills();

    for (size_t i = 1; i < services_.size(); ++i) {
      const std::vector<std::unique_ptr<skills::Skill>>& skills =
          services_[i]->GetSkills();

      if (!SkillsMatch(base_skills, skills, os)) {
        return false;
      }
    }
    return true;
  }

 private:
  bool SkillsMatch(const std::vector<std::unique_ptr<skills::Skill>>& skills1,
                   const std::vector<std::unique_ptr<skills::Skill>>& skills2,
                   std::ostream* os) {
    if (skills1.size() != skills2.size()) {
      *os << "Size mismatch: " << skills1.size() << " vs " << skills2.size();
      return false;
    }

    for (size_t i = 0; i < skills1.size(); ++i) {
      const skills::Skill& skill1 = *skills1[i];
      const skills::Skill& skill2 = *skills2[i];

      if (skill1.id != skill2.id) {
        *os << "Skill id mismatch: " << skill1.id << " vs " << skill2.id;
        return false;
      }

#define COMPARE_SKILL_FIELD(field)                                   \
  if (skill1.field != skill2.field) {                                \
    *os << "Skill " << skill1.name << " " << #field << " mismatch."; \
    return false;                                                    \
  }

      COMPARE_SKILL_FIELD(name);
      COMPARE_SKILL_FIELD(icon);
      COMPARE_SKILL_FIELD(prompt);
      COMPARE_SKILL_FIELD(description);
      COMPARE_SKILL_FIELD(source_skill_id);
      COMPARE_SKILL_FIELD(creation_time);
      COMPARE_SKILL_FIELD(last_update_time);

#undef COMPARE_SKILL_FIELD
    }
    return true;
  }

  const std::vector<skills::SkillsService*> services_;
  std::vector<
      std::unique_ptr<base::ScopedObservation<skills::SkillsService,
                                              skills::SkillsService::Observer>>>
      observations_;
};

class TwoClientSkillsSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientSkillsSyncTest() : SyncTest(TWO_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kSkillsEnabled};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_overrides_.InitWithFeatures(enabled_features, {});
  }
  ~TwoClientSkillsSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  skills::SkillsService& GetSkillsService(int index) const {
    skills::SkillsService* service =
        skills::SkillsServiceFactory::GetForProfile(GetProfile(index));
    CHECK(service);
    return *service;
  }

  size_t CountAllSkills(int index) const {
    return GetSkillsService(index).GetSkills().size();
  }

  std::vector<skills::SkillsService*> GetSkillsServices() {
    std::vector<skills::SkillsService*> services;
    for (Profile* profile : GetAllProfiles()) {
      skills::SkillsService* service =
          skills::SkillsServiceFactory::GetForProfile(profile);
      CHECK(service);
      services.push_back(service);
    }
    return services;
  }

 private:
  base::test::ScopedFeatureList feature_overrides_;
};

INSTANTIATE_TEST_SUITE_P(,
                         TwoClientSkillsSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(TwoClientSkillsSyncTest,
                       E2E_ENABLED(OneClientAddsSkill)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(SkillsMatchChecker(GetSkillsServices()).Wait())
      << "Initial skills did not match for all profiles";

  // Add one new skill to the first profile.
  GetSkillsService(0).AddSkill(/*source_skill_id=*/"source_id",
                               /*name=*/"Skill",
                               /*icon=*/"icon",
                               /*prompt=*/"prompt");

  EXPECT_TRUE(SkillsMatchChecker(GetSkillsServices()).Wait());
  EXPECT_EQ(CountAllSkills(1), 1u);
}

IN_PROC_BROWSER_TEST_P(TwoClientSkillsSyncTest,
                       E2E_ENABLED(TwoClientsAddSkills)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(SkillsMatchChecker(GetSkillsServices()).Wait())
      << "Initial skills did not match for all profiles";

  // Add one new skill per profile.
  GetSkillsService(0).AddSkill("source_skill_id_0", "Skill 0", "icon0",
                               "prompt 0");
  GetSkillsService(1).AddSkill("source_skill_id_1", "Skill 1", "icon1",
                               "prompt 1");

  EXPECT_TRUE(SkillsMatchChecker(GetSkillsServices()).Wait());
  EXPECT_EQ(CountAllSkills(0), 2u);
  EXPECT_EQ(CountAllSkills(1), 2u);
}

}  // namespace
