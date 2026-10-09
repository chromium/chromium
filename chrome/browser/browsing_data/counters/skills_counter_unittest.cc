// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/skills_counter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_factory.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/skills/mocks/mock_skills_service.h"
#include "components/skills/public/skill.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::NiceMock;
using ::testing::ReturnRef;

class SkillsCounterTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(skills_service_, GetSkills()).WillByDefault(ReturnRef(skills_));
  }

  void AddSkill(const std::string& id, base::Time last_update_time) {
    auto skill = std::make_unique<skills::Skill>(id, "name", "icon", "prompt");
    skill->creation_time = last_update_time;
    skill->last_update_time = last_update_time;
    skills_.push_back(std::move(skill));
  }

  std::unique_ptr<SkillsCounter> CreateCounter(
      syncer::SyncService* sync_service) {
    return std::make_unique<SkillsCounter>(skills_service_, sync_service);
  }

  browsing_data::BrowsingDataCounter::ResultInt RunCounter(
      SkillsCounter& counter) {
    finished_ = false;
    counter.Restart();
    EXPECT_TRUE(finished_);
    return result_;
  }

  void OnCounterResult(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    ASSERT_TRUE(result->Finished());
    finished_ = true;
    const auto* sync_result =
        static_cast<browsing_data::BrowsingDataCounter::SyncResult*>(
            result.get());
    result_ = sync_result->Value();
    sync_enabled_ = sync_result->is_sync_enabled();
  }

  browsing_data::BrowsingDataCounter::ResultCallback GetCallback() {
    return base::BindRepeating(&SkillsCounterTest::OnCounterResult,
                               base::Unretained(this));
  }

  bool finished() const { return finished_; }
  void reset_finished() { finished_ = false; }
  bool sync_enabled() const { return sync_enabled_; }
  syncer::TestSyncService& sync_service() { return sync_service_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::vector<std::unique_ptr<skills::Skill>> skills_;
  NiceMock<skills::MockSkillsService> skills_service_;
  syncer::TestSyncService sync_service_;
  bool finished_ = false;
  browsing_data::BrowsingDataCounter::ResultInt result_ = 0;
  bool sync_enabled_ = false;
};

TEST_F(SkillsCounterTest, PrefName) {
  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  EXPECT_STREQ(browsing_data::prefs::kDeleteSkills, counter->GetPrefName());
}

TEST_F(SkillsCounterTest, NoSkills) {
  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  counter->InitWithoutPref(base::Time(), GetCallback());
  EXPECT_EQ(0, RunCounter(*counter));
}

TEST_F(SkillsCounterTest, CountsAllSkillsForAllTime) {
  const base::Time now = base::Time::Now();
  AddSkill("skill1", now - base::Days(30));
  AddSkill("skill2", now - base::Hours(2));
  AddSkill("skill3", now);

  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  counter->InitWithoutPref(base::Time(), GetCallback());
  EXPECT_EQ(3, RunCounter(*counter));
}

TEST_F(SkillsCounterTest, CountsOnlySkillsModifiedInPeriod) {
  const base::Time now = base::Time::Now();
  AddSkill("skill1", now - base::Days(30));
  AddSkill("skill2", now - base::Days(2));
  AddSkill("skill3", now - base::Hours(2));
  AddSkill("skill4", now - base::Minutes(10));

  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  counter->InitWithoutPref(now - base::Hours(1), GetCallback());
  EXPECT_EQ(1, RunCounter(*counter));

  counter->SetBeginTime(now - base::Days(1));
  EXPECT_EQ(2, RunCounter(*counter));

  counter->SetBeginTime(now - base::Days(7));
  EXPECT_EQ(3, RunCounter(*counter));

  counter->SetBeginTime(base::Time());
  EXPECT_EQ(4, RunCounter(*counter));
}

TEST_F(SkillsCounterTest, BeginTimeIsInclusive) {
  const base::Time begin = base::Time::Now() - base::Hours(1);
  AddSkill("before", begin - base::Microseconds(1));
  AddSkill("at_begin", begin);

  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  counter->InitWithoutPref(begin, GetCallback());
  EXPECT_EQ(1, RunCounter(*counter));
}

// The result should report that skills are synced when the SKILL sync data
// type is active at initialization time.
TEST_F(SkillsCounterTest, SyncedWhenSkillTypeActive) {
  AddSkill("skill", base::Time::Now());
  // TestSyncService is signed in with all types active by default.
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::SKILL));

  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  counter->InitWithoutPref(base::Time(), GetCallback());
  EXPECT_EQ(1, RunCounter(*counter));
  EXPECT_TRUE(sync_enabled());
}

TEST_F(SkillsCounterTest, NotSyncedWhenSignedOut) {
  AddSkill("skill", base::Time::Now());
  sync_service().SetSignedOut();
  ASSERT_FALSE(sync_service().GetActiveDataTypes().Has(syncer::SKILL));

  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  counter->InitWithoutPref(base::Time(), GetCallback());
  EXPECT_EQ(1, RunCounter(*counter));
  EXPECT_FALSE(sync_enabled());
}

// Sync being on as a whole is not enough; the SKILL type itself must be
// active.
TEST_F(SkillsCounterTest, NotSyncedWhenSkillTypeInactive) {
  AddSkill("skill", base::Time::Now());
  sync_service().SetFailedDataTypes({syncer::SKILL});
  ASSERT_FALSE(sync_service().GetActiveDataTypes().Has(syncer::SKILL));
  ASSERT_FALSE(sync_service().GetActiveDataTypes().empty());

  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  counter->InitWithoutPref(base::Time(), GetCallback());
  EXPECT_EQ(1, RunCounter(*counter));
  EXPECT_FALSE(sync_enabled());
}

// A change in sync state should restart the counter so that the UI picks up
// the new sync state.
TEST_F(SkillsCounterTest, RestartsOnSyncStateChange) {
  std::unique_ptr<SkillsCounter> counter = CreateCounter(&sync_service());
  counter->InitWithoutPref(base::Time(), GetCallback());
  RunCounter(*counter);
  EXPECT_TRUE(sync_enabled());

  sync_service().SetSignedOut();
  reset_finished();
  sync_service().FireStateChanged();
  EXPECT_TRUE(finished());
  EXPECT_FALSE(sync_enabled());

  sync_service().SetSignedIn(signin::ConsentLevel::kSignin);
  reset_finished();
  sync_service().FireStateChanged();
  EXPECT_TRUE(finished());
  EXPECT_TRUE(sync_enabled());
}

TEST_F(SkillsCounterTest, NoSyncService) {
  AddSkill("skill", base::Time::Now());
  std::unique_ptr<SkillsCounter> counter = CreateCounter(nullptr);
  counter->InitWithoutPref(base::Time(), GetCallback());
  EXPECT_EQ(1, RunCounter(*counter));
  EXPECT_FALSE(sync_enabled());
}

class SkillsCounterFactoryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(SkillsCounterFactoryTest, NoCounterWithoutSkillsService) {
  skills::SkillsServiceFactory::GetInstance()->SetTestingFactory(
      &profile_,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return nullptr;
          }));
  EXPECT_FALSE(BrowsingDataCounterFactory::GetForProfileAndPref(
      &profile_, browsing_data::prefs::kDeleteSkills));
}

TEST_F(SkillsCounterFactoryTest, CreatesCounterWithSkillsService) {
  skills::SkillsServiceFactory::GetInstance()->SetTestingFactory(
      &profile_,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<NiceMock<skills::MockSkillsService>>();
          }));
  std::unique_ptr<browsing_data::BrowsingDataCounter> counter =
      BrowsingDataCounterFactory::GetForProfileAndPref(
          &profile_, browsing_data::prefs::kDeleteSkills);
  ASSERT_TRUE(counter);
  EXPECT_STREQ(browsing_data::prefs::kDeleteSkills, counter->GetPrefName());
}

}  // namespace
