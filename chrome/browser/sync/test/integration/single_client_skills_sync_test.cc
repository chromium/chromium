// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Contains;
using testing::Pointee;
using testing::SizeIs;

MATCHER_P3(HasSkill, name, icon, prompt, "") {
  return arg.name == name && arg.icon == icon && arg.prompt == prompt;
}

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

}  // namespace
