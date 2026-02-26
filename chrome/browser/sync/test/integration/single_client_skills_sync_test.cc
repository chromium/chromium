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
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/unknown_field_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using syncer::test::HasUnknownField;
using testing::AllOf;
using testing::Contains;
using testing::IsEmpty;
using testing::Pointee;
using testing::SizeIs;
using testing::UnorderedElementsAre;

MATCHER_P(HasSimpleSkill, matcher, "") {
  return arg.has_simple_skill() &&
         testing::ExplainMatchResult(matcher, arg.simple_skill(),
                                     result_listener);
}

MATCHER_P5(HasSkill, source_skill_id, name, icon, prompt, description, "") {
  return arg.source_skill_id == source_skill_id && arg.name == name &&
         arg.icon == icon && arg.prompt == prompt &&
         arg.description == description;
}

MATCHER_P6(HasSkillSpecifics,
           guid,
           source_skill_id,
           name,
           icon,
           prompt,
           description,
           "") {
  return arg.guid() == guid && arg.source_skill_id() == source_skill_id &&
         arg.name() == name && arg.icon() == icon &&
         arg.simple_skill().prompt() == prompt &&
         arg.simple_skill().description() == description;
}

sync_pb::SkillSpecifics CreateSkillSpecifics(std::string guid,
                                             std::string source_skill_id,
                                             std::string name,
                                             std::string icon,
                                             std::string prompt,
                                             std::string description) {
  sync_pb::SkillSpecifics specifics;
  specifics.set_guid(std::move(guid));
  specifics.set_source_skill_id(std::move(source_skill_id));
  specifics.set_name(std::move(name));
  specifics.set_icon(std::move(icon));
  specifics.mutable_simple_skill()->set_prompt(std::move(prompt));
  specifics.mutable_simple_skill()->set_description(std::move(description));
  specifics.set_skill_source(sync_pb::SKILL_SOURCE_USER_CREATED);
  specifics.set_schema_version(1);
  return specifics;
}

std::vector<sync_pb::SkillSpecifics> SyncEntitiesToSkillSpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::SkillSpecifics> skills;
  for (sync_pb::SyncEntity& entity : entities) {
    CHECK(entity.specifics().has_skill());
    sync_pb::SkillSpecifics specifics;
    specifics.Swap(entity.mutable_specifics()->mutable_skill());
    skills.push_back(std::move(specifics));
  }
  return skills;
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
  void OnSkillUpdated(std::string_view skill_id,
                      skills::SkillsService::UpdateSource update_source,
                      bool is_position_changed) override {
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

// A checker that waits for the skills in the fake server to match the provided
// matcher.
class ServerSkillsMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::SkillSpecifics>>;

  explicit ServerSkillsMatchChecker(const Matcher& matcher)
      : matcher_(matcher) {}

  // fake_server::FakeServerMatchStatusChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for server skills to match. ";

    testing::StringMatchResultListener result_listener;
    bool matches = testing::ExplainMatchResult(
        matcher_,
        SyncEntitiesToSkillSpecifics(
            fake_server()->GetSyncEntitiesByDataType(syncer::SKILL)),
        &result_listener);
    *os << result_listener.str();
    return matches;
  }

 private:
  const Matcher matcher_;
};

class SingleClientSkillsSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientSkillsSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kSkillsEnabled};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_overrides_.InitWithFeatures(enabled_features, {});
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
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

  void InjectTombstoneToFakeServer(const std::string& skill_id) {
    syncer::ClientTagHash client_tag_hash =
        syncer::ClientTagHash::FromUnhashed(syncer::SKILL, skill_id);

    fake_server_->InjectEntity(syncer::PersistentTombstoneEntity::CreateNew(
        syncer::LoopbackServerEntity::CreateId(syncer::SKILL,
                                               client_tag_hash.value()),
        client_tag_hash.value()));
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

  GetSkillsService().AddSkill(/*source_skill_id=*/"source_skill_id",
                              /*name=*/"test_skill",
                              /*icon=*/"icon",
                              /*prompt=*/"prompt");
}

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest, ShouldLoadDataOnRestart) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::SKILL));
  EXPECT_THAT(GetSkillsService().GetSkills(),
              Contains(Pointee(HasSkill("source_skill_id", "test_skill", "icon",
                                        "prompt",
                                        /*description=*/""))));
}

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest, ShouldApplyRemoteUpdates) {
  ASSERT_TRUE(SetupSync());

  const skills::Skill* skill1 = GetSkillsService().AddSkill(
      /*source_skill_id=*/"source_skill_id", /*name=*/"skill1",
      /*icon=*/"icon1",
      /*prompt=*/"prompt1");
  const skills::Skill* skill_to_update = GetSkillsService().AddSkill(
      /*source_skill_id=*/"", /*name=*/"skill2 to update",
      /*icon=*/"icon2",
      /*prompt=*/"prompt2");
  const skills::Skill* skill_to_delete = GetSkillsService().AddSkill(
      /*source_skill_id=*/"", /*name=*/"skill3 to delete",
      /*icon=*/"icon3",
      /*prompt=*/"prompt3");
  const std::string skill_id_to_add =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  ASSERT_TRUE(
      ServerSkillsMatchChecker(
          UnorderedElementsAre(
              HasSkillSpecifics(skill1->id, "source_skill_id", "skill1",
                                "icon1", "prompt1",
                                /*description=*/""),
              HasSkillSpecifics(skill_to_update->id, "", "skill2 to update",
                                "icon2", "prompt2", /*description=*/""),
              HasSkillSpecifics(skill_to_delete->id, "", "skill3 to delete",
                                "icon3", "prompt3", /*description=*/"")))
          .Wait());

  InjectSpecificsToFakeServer(CreateSkillSpecifics(
      skill_to_update->id, "", "updated name", "updated icon", "updated prompt",
      "updated description"));
  InjectSpecificsToFakeServer(CreateSkillSpecifics(
      skill_id_to_add, "new_source_skill_id", "new_skill_name",
      "new_skill_icon", "new_skill_prompt", "new_skill_description"));
  InjectTombstoneToFakeServer(skill_to_delete->id);

  EXPECT_TRUE(
      SkillsServiceChecker(
          GetSkillsService(),
          UnorderedElementsAre(
              Pointee(HasSkill("source_skill_id", "skill1", "icon1", "prompt1",
                               /*description=*/"")),
              Pointee(HasSkill("", "updated name", "updated icon",
                               "updated prompt", "updated description")),
              Pointee(HasSkill("new_source_skill_id", "new_skill_name",
                               "new_skill_icon", "new_skill_prompt",
                               "new_skill_description"))))
          .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest, ShouldMergeRemoteData) {
  InjectSpecificsToFakeServer(CreateSkillSpecifics(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), "source_skill_id1",
      "skill1", "icon1", "prompt1", "description1"));
  InjectSpecificsToFakeServer(CreateSkillSpecifics(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), "source_skill_id2",
      "skill2", "icon2", "prompt2", "description2"));

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(SkillsServiceChecker(
                  GetSkillsService(),
                  UnorderedElementsAre(
                      Pointee(HasSkill("source_skill_id1", "skill1", "icon1",
                                       "prompt1", "description1")),
                      Pointee(HasSkill("source_skill_id2", "skill2", "icon2",
                                       "prompt2", "description2"))))
                  .Wait());
}

// TODO(crbug.com/471795213): add a test to verify that skills can't be created
// when sync is disabled.

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest,
                       ShouldPreserveUnknownFields) {
  const std::string kSkillId =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  sync_pb::SkillSpecifics specifics_with_unknown_fields = CreateSkillSpecifics(
      kSkillId, "source_skill_id", "name", "icon", "prompt", "description");

  // Add unknown fields that should be preserved by the client.
  syncer::test::AddUnknownFieldToProto(
      *specifics_with_unknown_fields.mutable_simple_skill(),
      "simple_skill_unknown_field");
  syncer::test::AddUnknownFieldToProto(specifics_with_unknown_fields,
                                       "specifics_unknown_field");

  InjectSpecificsToFakeServer(specifics_with_unknown_fields);

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(SkillsServiceChecker(GetSkillsService(),
                                   UnorderedElementsAre(Pointee(HasSkill(
                                       "source_skill_id", "name", "icon",
                                       "prompt", "description"))))
                  .Wait());

  // Update the skill on the client.
  const skills::Skill* skill = GetSkillsService().UpdateSkill(
      kSkillId, "updated name", "updated icon", "updated prompt");
  ASSERT_NE(skill, nullptr);

  // Verify that the server received the update and that unknown fields are
  // preserved.
  EXPECT_TRUE(
      ServerSkillsMatchChecker(
          UnorderedElementsAre(AllOf(
              HasSkillSpecifics(kSkillId, "source_skill_id", "updated name",
                                "updated icon", "updated prompt",
                                /*description=*/""),
              HasUnknownField("specifics_unknown_field"),
              HasSimpleSkill(HasUnknownField("simple_skill_unknown_field")))))
          .Wait());
}

// ChromeOS does not support signout.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest,
                       ShouldDeleteAllDataOnDisableSync) {
  InjectSpecificsToFakeServer(CreateSkillSpecifics(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), "source_skill_id1",
      "skill1", "icon1", "prompt1", "description1"));
  InjectSpecificsToFakeServer(CreateSkillSpecifics(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), "source_skill_id2",
      "skill2", "icon2", "prompt2", "description2"));

  ASSERT_TRUE(SetupSync());

  ASSERT_THAT(GetSkillsService().GetSkills(),
              UnorderedElementsAre(
                  Pointee(HasSkill("source_skill_id1", "skill1", "icon1",
                                   "prompt1", "description1")),
                  Pointee(HasSkill("source_skill_id2", "skill2", "icon2",
                                   "prompt2", "description2"))));

  // Sign out the primary account to disable sync and verify that all data was
  // deleted.
  GetClient(0)->SignOutPrimaryAccount();

  EXPECT_TRUE(SkillsServiceChecker(GetSkillsService(), IsEmpty()).Wait());

  // Sign in again to re-enable sync and verify that the data was re-synced.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());

  EXPECT_TRUE(SkillsServiceChecker(
                  GetSkillsService(),
                  UnorderedElementsAre(
                      Pointee(HasSkill("source_skill_id1", "skill1", "icon1",
                                       "prompt1", "description1")),
                      Pointee(HasSkill("source_skill_id2", "skill2", "icon2",
                                       "prompt2", "description2"))))
                  .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
