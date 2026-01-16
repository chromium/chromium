// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Contains;
using testing::Pointee;
using testing::SizeIs;
using testing::UnorderedElementsAre;

MATCHER_P3(HasSkill, name, icon, prompt, "") {
  return arg.name == name && arg.icon == icon && arg.prompt == prompt;
}

MATCHER_P4(HasSkillSpecifics, guid, name, icon, prompt, "") {
  return arg.guid() == guid && arg.name() == name && arg.icon() == icon &&
         arg.simple_skill().prompt() == prompt;
}

sync_pb::SkillSpecifics CreateSkillSpecifics(std::string guid,
                                             std::string name,
                                             std::string icon,
                                             std::string prompt) {
  sync_pb::SkillSpecifics specifics;
  specifics.set_guid(std::move(guid));
  specifics.set_name(std::move(name));
  specifics.set_icon(std::move(icon));
  specifics.mutable_simple_skill()->set_prompt(std::move(prompt));
  return specifics;
}

// A checker that waits for the skills in the service on the client to match the
// provided matcher.
class SkillsServiceChecker : public StatusChangeChecker,
                             public skills::SkillsService::Observer {
 public:
  using Matcher = testing::Matcher<std::vector<std::unique_ptr<skills::Skill>>>;

  SkillsServiceChecker(skills::SkillsService& skills_service,
                       const Matcher& skills_matcher)
      : skills_service_(skills_service), skills_matcher_(skills_matcher) {
    skills_service_observation_.Observe(&skills_service);
  }

  // skills::SkillsService::Observer overrides.
  void OnSkillUpdated(
      const std::string& skill_id,
      skills::SkillsService::UpdateSource update_source) override {
    CheckExitCondition();
  }

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for skills to match. ";

    testing::StringMatchResultListener result_listener;
    bool matches = testing::ExplainMatchResult(
        skills_matcher_, skills_service_->GetSkills(), &result_listener);
    *os << result_listener.str();
    return matches;
  }

 private:
  base::ScopedObservation<skills::SkillsService,
                          skills::SkillsService::Observer>
      skills_service_observation_{this};

  base::raw_ref<skills::SkillsService> skills_service_;
  const Matcher skills_matcher_;
};

class SingleClientSkillsSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientSkillsSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {syncer::kSyncSkill};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_overrides_.InitWithFeatures(enabled_features, {});
  }

  skills::SkillsService& GetSkillsService() const {
    skills::SkillsService* service =
        skills::SkillsServiceFactory::GetForProfile(GetProfile(0));
    CHECK(service);
    return *service;
  }

  void InjectSpecificsToFakeServer(const sync_pb::SkillSpecifics& specifics) {
    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_skill() = specifics;
    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/specifics.guid(),
            /*client_tag=*/specifics.guid(), entity_specifics,
            /*creation_time=*/0,
            /*last_modified_time=*/0));
  }

 private:
  base::test::ScopedFeatureList feature_overrides_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientSkillsSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest, ShouldInitializeDataType) {
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::SKILL));
}

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest,
                       PRE_ShouldLoadDataOnRestart) {
  ASSERT_TRUE(SetupSync());

  GetSkillsService().AddSkill(/*name=*/"test_skill", /*icon=*/"icon",
                              /*prompt=*/"prompt");
}

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest, ShouldLoadDataOnRestart) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::SKILL));
  EXPECT_THAT(GetSkillsService().GetSkills(),
              Contains(Pointee(HasSkill("test_skill", "icon", "prompt"))));
}

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest, ShouldApplyRemoteUpdates) {
  ASSERT_TRUE(SetupSync());

  GetSkillsService().AddSkill(/*name=*/"skill1", /*icon=*/"icon1",
                              /*prompt=*/"prompt1");
  const skills::Skill* skill_to_update =
      GetSkillsService().AddSkill(/*name=*/"skill2 to update", /*icon=*/"icon2",
                                  /*prompt=*/"prompt2");
  const std::string skill_id_to_add =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  InjectSpecificsToFakeServer(CreateSkillSpecifics(
      skill_to_update->id, "updated name", "updated icon", "updated prompt"));
  InjectSpecificsToFakeServer(CreateSkillSpecifics(
      skill_id_to_add, "new_skill_name", "new_skill_icon", "new_skill_prompt"));
  // TODO(crbug.com/471795213): test deletion propagation.

  EXPECT_TRUE(SkillsServiceChecker(
                  GetSkillsService(),
                  UnorderedElementsAre(
                      Pointee(HasSkill("skill1", "icon1", "prompt1")),
                      Pointee(HasSkill("updated name", "updated icon",
                                       "updated prompt")),
                      Pointee(HasSkill("new_skill_name", "new_skill_icon",
                                       "new_skill_prompt"))))
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest, ShouldMergeRemoteData) {
  InjectSpecificsToFakeServer(
      CreateSkillSpecifics(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           "skill1", "icon1", "prompt1"));
  InjectSpecificsToFakeServer(
      CreateSkillSpecifics(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           "skill2", "icon2", "prompt2"));

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(
      SkillsServiceChecker(
          GetSkillsService(),
          UnorderedElementsAre(Pointee(HasSkill("skill1", "icon1", "prompt1")),
                               Pointee(HasSkill("skill2", "icon2", "prompt2"))))
          .Wait());
}

}  // namespace
