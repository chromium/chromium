// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"

#include <stddef.h>
#include <utility>

#include "base/base_switches.h"
#include "base/build_time.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "base/test/task_environment.h"
#include "base/test/test_shared_memory_util.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_ANDROID)
#include "base/posix/global_descriptors.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mach_port_rendezvous.h"
#endif

namespace base {

namespace {

// Default group name used by several tests.
const char kDefaultGroupName[] = "DefaultGroup";

// Call FieldTrialList::FactoryGetFieldTrial().
scoped_refptr<FieldTrial> CreateFieldTrial(
    const std::string& trial_name,
    int total_probability,
    const std::string& default_group_name,
    int* default_group_number) {
  return FieldTrialList::FactoryGetFieldTrial(
      trial_name, total_probability, default_group_name,
      FieldTrial::SESSION_RANDOMIZED, default_group_number);
}

// A FieldTrialList::Observer implementation which stores the trial name and
// group name received via OnFieldTrialGroupFinalized() for later inspection.
class TestFieldTrialObserver : public FieldTrialList::Observer {
 public:
  TestFieldTrialObserver() { FieldTrialList::AddObserver(this); }
  TestFieldTrialObserver(const TestFieldTrialObserver&) = delete;
  TestFieldTrialObserver& operator=(const TestFieldTrialObserver&) = delete;

  ~TestFieldTrialObserver() override { FieldTrialList::RemoveObserver(this); }

  void OnFieldTrialGroupFinalized(const std::string& trial,
                                  const std::string& group) override {
    trial_name_ = trial;
    group_name_ = group;
  }

  const std::string& trial_name() const { return trial_name_; }
  const std::string& group_name() const { return group_name_; }

 private:
  std::string trial_name_;
  std::string group_name_;
};

// A FieldTrialList::Observer implementation which accesses the group of a
// FieldTrial from OnFieldTrialGroupFinalized(). Used to test reentrancy.
class FieldTrialObserverAccessingGroup : public FieldTrialList::Observer {
 public:
  // |trial_to_access| is the FieldTrial on which to invoke group() when
  // receiving an OnFieldTrialGroupFinalized() notification.
  explicit FieldTrialObserverAccessingGroup(
      scoped_refptr<FieldTrial> trial_to_access)
      : trial_to_access_(trial_to_access) {
    FieldTrialList::AddObserver(this);
  }
  FieldTrialObserverAccessingGroup(const FieldTrialObserverAccessingGroup&) =
      delete;
  FieldTrialObserverAccessingGroup& operator=(
      const FieldTrialObserverAccessingGroup&) = delete;

  ~FieldTrialObserverAccessingGroup() override {
    FieldTrialList::RemoveObserver(this);
  }

  void OnFieldTrialGroupFinalized(const std::string& trial,
                                  const std::string& group) override {
    trial_to_access_->group();
  }

 private:
  scoped_refptr<FieldTrial> trial_to_access_;
};

std::string MockEscapeQueryParamValue(const std::string& input) {
  return input;
}

}  // namespace

class FieldTrialTest : public ::testing::Test {
 public:
  FieldTrialTest() : trial_list_(nullptr) {}
  FieldTrialTest(const FieldTrialTest&) = delete;
  FieldTrialTest& operator=(const FieldTrialTest&) = delete;

 private:
  test::TaskEnvironment task_environment_;
  // The test suite instantiates a FieldTrialList but for the purpose of these
  // tests it's cleaner to start from scratch.
  test::ScopedFieldTrialListResetter trial_list_resetter_;
  FieldTrialList trial_list_;
};

// Test registration, and also check that destructors are called for trials.
TEST_F(FieldTrialTest, Registration) {
  const char name1[] = "name 1 test";
  const char name2[] = "name 2 test";
  EXPECT_FALSE(FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  scoped_refptr<FieldTrial> trial1 =
      CreateFieldTrial(name1, 10, "default name 1 test", nullptr);
  EXPECT_EQ(FieldTrial::kNotFinalized, trial1->group_);
  EXPECT_EQ(name1, trial1->trial_name());
  EXPECT_EQ("", trial1->group_name_internal());

  trial1->AppendGroup(std::string(), 7);

  EXPECT_EQ(trial1.get(), FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  scoped_refptr<FieldTrial> trial2 =
      CreateFieldTrial(name2, 10, "default name 2 test", nullptr);
  EXPECT_EQ(FieldTrial::kNotFinalized, trial2->group_);
  EXPECT_EQ(name2, trial2->trial_name());
  EXPECT_EQ("", trial2->group_name_internal());

  trial2->AppendGroup("a first group", 7);

  EXPECT_EQ(trial1.get(), FieldTrialList::Find(name1));
  EXPECT_EQ(trial2.get(), FieldTrialList::Find(name2));
  // Note: FieldTrialList should delete the objects at shutdown.
}

TEST_F(FieldTrialTest, AbsoluteProbabilities) {
  char always_true[] = " always true";
  char default_always_true[] = " default always true";
  char always_false[] = " always false";
  char default_always_false[] = " default always false";
  for (int i = 1; i < 250; ++i) {
    // Try lots of names, by changing the first character of the name.
    char c = static_cast<char>(i);
    always_true[0] = c;
    default_always_true[0] = c;
    always_false[0] = c;
    default_always_false[0] = c;

    scoped_refptr<FieldTrial> trial_true =
        CreateFieldTrial(always_true, 10, default_always_true, nullptr);
    const std::string winner = "TheWinner";
    int winner_group = trial_true->AppendGroup(winner, 10);

    EXPECT_EQ(winner_group, trial_true->group());
    EXPECT_EQ(winner, trial_true->group_name());

    scoped_refptr<FieldTrial> trial_false =
        CreateFieldTrial(always_false, 10, default_always_false, nullptr);
    int loser_group = trial_false->AppendGroup("ALoser", 0);

    EXPECT_NE(loser_group, trial_false->group());
  }
}

TEST_F(FieldTrialTest, RemainingProbability) {
  // First create a test that hasn't had a winner yet.
  const std::string winner = "Winner";
  const std::string loser = "Loser";
  scoped_refptr<FieldTrial> trial;
  int counter = 0;
  int default_group_number = -1;
  do {
    std::string name = StringPrintf("trial%d", ++counter);
    trial = CreateFieldTrial(name, 10, winner, &default_group_number);
    trial->AppendGroup(loser, 5);  // 50% chance of not being chosen.
    // If a group is not assigned, group_ will be kNotFinalized.
  } while (trial->group_ != FieldTrial::kNotFinalized);

  // And that 'default' group (winner) should always win.
  EXPECT_EQ(default_group_number, trial->group());

  // And that winner should ALWAYS win.
  EXPECT_EQ(winner, trial->group_name());
}

TEST_F(FieldTrialTest, FiftyFiftyProbability) {
  // Check that even with small divisors, we have the proper probabilities, and
  // all outcomes are possible.  Since this is a 50-50 test, it should get both
  // outcomes in a few tries, but we'll try no more than 100 times (and be flaky
  // with probability around 1 in 2^99).
  bool first_winner = false;
  bool second_winner = false;
  int counter = 0;
  do {
    std::string name = StringPrintf("FiftyFifty%d", ++counter);
    std::string default_group_name =
        StringPrintf("Default FiftyFifty%d", ++counter);
    scoped_refptr<FieldTrial> trial =
        CreateFieldTrial(name, 2, default_group_name, nullptr);
    trial->AppendGroup("first", 1);  // 50% chance of being chosen.
    // If group_ is kNotFinalized, then a group assignement hasn't been done.
    if (trial->group_ != FieldTrial::kNotFinalized) {
      first_winner = true;
      continue;
    }
    trial->AppendGroup("second", 1);  // Always chosen at this point.
    EXPECT_NE(FieldTrial::kNotFinalized, trial->group());
    second_winner = true;
  } while ((!second_winner || !first_winner) && counter < 100);
  EXPECT_TRUE(second_winner);
  EXPECT_TRUE(first_winner);
}

TEST_F(FieldTrialTest, MiddleProbabilities) {
  char name[] = " same name";
  char default_group_name[] = " default same name";
  bool false_event_seen = false;
  bool true_event_seen = false;
  for (int i = 1; i < 250; ++i) {
    char c = static_cast<char>(i);
    name[0] = c;
    default_group_name[0] = c;
    scoped_refptr<FieldTrial> trial =
        CreateFieldTrial(name, 10, default_group_name, nullptr);
    int might_win = trial->AppendGroup("MightWin", 5);

    if (trial->group() == might_win) {
      true_event_seen = true;
    } else {
      false_event_seen = true;
    }
    if (false_event_seen && true_event_seen)
      return;  // Successful test!!!
  }
  // Very surprising to get here. Probability should be around 1 in 2 ** 250.
  // One of the following will fail.
  EXPECT_TRUE(false_event_seen);
  EXPECT_TRUE(true_event_seen);
}

TEST_F(FieldTrialTest, OneWinner) {
  char name[] = "Some name";
  char default_group_name[] = "Default some name";
  int group_count(10);

  int default_group_number = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(name, group_count, default_group_name, nullptr);
  int winner_index(-2);
  std::string winner_name;

  for (int i = 1; i <= group_count; ++i) {
    int might_win = trial->AppendGroup(std::string(), 1);

    // Because we keep appending groups, we want to see if the last group that
    // was added has been assigned or not.
    if (trial->group_ == might_win) {
      EXPECT_EQ(-2, winner_index);
      winner_index = might_win;
      StringAppendF(&winner_name, "%d", might_win);
      EXPECT_EQ(winner_name, trial->group_name());
    }
  }
  EXPECT_GE(winner_index, 0);
  // Since all groups cover the total probability, we should not have
  // chosen the default group.
  EXPECT_NE(trial->group(), default_group_number);
  EXPECT_EQ(trial->group(), winner_index);
  EXPECT_EQ(trial->group_name(), winner_name);
}

TEST_F(FieldTrialTest, DisableProbability) {
  const std::string default_group_name = "Default group";
  const std::string loser = "Loser";
  const std::string name = "Trial";

  // Create a field trail that is disabled.
  int default_group_number = -1;
  FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
      name, 1000000000, default_group_name, FieldTrial::SESSION_RANDOMIZED,
      &default_group_number);
  trial->Disable();
  trial->AppendGroup(loser, 999999999);  // 99.9999999% chance of being chosen.

  // Because trial has expired, we should always be in the default group.
  EXPECT_EQ(default_group_number, trial->group());

  // And that default_group_name should ALWAYS win.
  EXPECT_EQ(default_group_name, trial->group_name());
}

TEST_F(FieldTrialTest, ActiveGroups) {
  std::string no_group("No Group");
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(no_group, 10, "Default", nullptr);

  // There is no winner yet, so no NameGroupId should be returned.
  FieldTrial::ActiveGroup active_group;
  EXPECT_FALSE(trial->GetActiveGroup(&active_group));

  // Create a single winning group.
  std::string one_winner("One Winner");
  trial = CreateFieldTrial(one_winner, 10, "Default", nullptr);
  std::string winner("Winner");
  trial->AppendGroup(winner, 10);
  EXPECT_FALSE(trial->GetActiveGroup(&active_group));
  // Finalize the group selection by accessing the selected group.
  trial->group();
  EXPECT_TRUE(trial->GetActiveGroup(&active_group));
  EXPECT_EQ(one_winner, active_group.trial_name);
  EXPECT_EQ(winner, active_group.group_name);

  std::string multi_group("MultiGroup");
  scoped_refptr<FieldTrial> multi_group_trial =
      CreateFieldTrial(multi_group, 9, "Default", nullptr);

  multi_group_trial->AppendGroup("Me", 3);
  multi_group_trial->AppendGroup("You", 3);
  multi_group_trial->AppendGroup("Them", 3);
  EXPECT_FALSE(multi_group_trial->GetActiveGroup(&active_group));
  // Finalize the group selection by accessing the selected group.
  multi_group_trial->group();
  EXPECT_TRUE(multi_group_trial->GetActiveGroup(&active_group));
  EXPECT_EQ(multi_group, active_group.trial_name);
  EXPECT_EQ(multi_group_trial->group_name(), active_group.group_name);

  // Now check if the list is built properly...
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(2U, active_groups.size());
  for (size_t i = 0; i < active_groups.size(); ++i) {
    // Order is not guaranteed, so check all values.
    EXPECT_NE(no_group, active_groups[i].trial_name);
    EXPECT_TRUE(one_winner != active_groups[i].trial_name ||
                winner == active_groups[i].group_name);
    EXPECT_TRUE(multi_group != active_groups[i].trial_name ||
                multi_group_trial->group_name() == active_groups[i].group_name);
  }
}

TEST_F(FieldTrialTest, GetActiveFieldTrialGroupsFromString) {
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroupsFromString("*A/X/B/Y/*C/Z",
                                                      &active_groups);
  ASSERT_EQ(2U, active_groups.size());
  EXPECT_EQ("A", active_groups[0].trial_name);
  EXPECT_EQ("X", active_groups[0].group_name);
  EXPECT_EQ("C", active_groups[1].trial_name);
  EXPECT_EQ("Z", active_groups[1].group_name);
}

TEST_F(FieldTrialTest, ActiveGroupsNotFinalized) {
  const char kTrialName[] = "TestTrial";
  const char kSecondaryGroupName[] = "SecondaryGroup";

  int default_group = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  const int secondary_group = trial->AppendGroup(kSecondaryGroupName, 50);

  // Before |group()| is called, |GetActiveGroup()| should return false.
  FieldTrial::ActiveGroup active_group;
  EXPECT_FALSE(trial->GetActiveGroup(&active_group));

  // |GetActiveFieldTrialGroups()| should also not include the trial.
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_TRUE(active_groups.empty());

  // After |group()| has been called, both APIs should succeed.
  const int chosen_group = trial->group();
  EXPECT_TRUE(chosen_group == default_group || chosen_group == secondary_group);

  EXPECT_TRUE(trial->GetActiveGroup(&active_group));
  EXPECT_EQ(kTrialName, active_group.trial_name);
  if (chosen_group == default_group)
    EXPECT_EQ(kDefaultGroupName, active_group.group_name);
  else
    EXPECT_EQ(kSecondaryGroupName, active_group.group_name);

  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  ASSERT_EQ(1U, active_groups.size());
  EXPECT_EQ(kTrialName, active_groups[0].trial_name);
  EXPECT_EQ(active_group.group_name, active_groups[0].group_name);
}

TEST_F(FieldTrialTest, GetGroupNameWithoutActivation) {
  const char kTrialName[] = "TestTrial";
  const char kSecondaryGroupName[] = "SecondaryGroup";

  int default_group = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  trial->AppendGroup(kSecondaryGroupName, 50);

  // The trial should start inactive.
  EXPECT_FALSE(FieldTrialList::IsTrialActive(kTrialName));

  // Calling |GetGroupNameWithoutActivation()| should not activate the trial.
  std::string group_name = trial->GetGroupNameWithoutActivation();
  EXPECT_FALSE(group_name.empty());
  EXPECT_FALSE(FieldTrialList::IsTrialActive(kTrialName));

  // Calling |group_name()| should activate it and return the same group name.
  EXPECT_EQ(group_name, trial->group_name());
  EXPECT_TRUE(FieldTrialList::IsTrialActive(kTrialName));
}

TEST_F(FieldTrialTest, Save) {
  std::string save_string;

  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial("Some name", 10, "Default some name", nullptr);
  // There is no winner yet, so no textual group name is associated with trial.
  // In this case, the trial should not be included.
  EXPECT_EQ("", trial->group_name_internal());
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("", save_string);
  save_string.clear();

  // Create a winning group.
  trial->AppendGroup("Winner", 10);
  // Finalize the group selection by accessing the selected group.
  trial->group();
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/", save_string);
  save_string.clear();

  // Create a second trial and winning group.
  scoped_refptr<FieldTrial> trial2 =
      CreateFieldTrial("xxx", 10, "Default xxx", nullptr);
  trial2->AppendGroup("yyyy", 10);
  // Finalize the group selection by accessing the selected group.
  trial2->group();

  FieldTrialList::StatesToString(&save_string);
  // We assume names are alphabetized... though this is not critical.
  EXPECT_EQ("Some name/Winner/xxx/yyyy/", save_string);
  save_string.clear();

  // Create a third trial with only the default group.
  scoped_refptr<FieldTrial> trial3 =
      CreateFieldTrial("zzz", 10, "default", nullptr);
  // Finalize the group selection by accessing the selected group.
  trial3->group();

  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/xxx/yyyy/zzz/default/", save_string);
}

TEST_F(FieldTrialTest, SaveAll) {
  std::string save_string;

  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial("Some name", 10, "Default some name", nullptr);
  EXPECT_EQ("", trial->group_name_internal());
  FieldTrialList::AllStatesToString(&save_string, false);
  EXPECT_EQ("Some name/Default some name/", save_string);
  // Getting all states should have finalized the trial.
  EXPECT_EQ("Default some name", trial->group_name_internal());
  save_string.clear();

  // Create a winning group.
  trial = CreateFieldTrial("trial2", 10, "Default some name", nullptr);
  trial->AppendGroup("Winner", 10);
  // Finalize the group selection by accessing the selected group.
  trial->group();
  FieldTrialList::AllStatesToString(&save_string, false);
  EXPECT_EQ("Some name/Default some name/*trial2/Winner/", save_string);
  save_string.clear();

  // Create a second trial and winning group.
  scoped_refptr<FieldTrial> trial2 =
      CreateFieldTrial("xxx", 10, "Default xxx", nullptr);
  trial2->AppendGroup("yyyy", 10);
  // Finalize the group selection by accessing the selected group.
  trial2->group();

  FieldTrialList::AllStatesToString(&save_string, false);
  // We assume names are alphabetized... though this is not critical.
  EXPECT_EQ("Some name/Default some name/*trial2/Winner/*xxx/yyyy/",
            save_string);
  save_string.clear();

  // Create a third trial with only the default group.
  scoped_refptr<FieldTrial> trial3 =
      CreateFieldTrial("zzz", 10, "default", nullptr);

  FieldTrialList::AllStatesToString(&save_string, false);
  EXPECT_EQ("Some name/Default some name/*trial2/Winner/*xxx/yyyy/zzz/default/",
            save_string);

  // Create disabled study.
  int default_group_number = -1;
  scoped_refptr<FieldTrial> disabled_trial =
      FieldTrialList::FactoryGetFieldTrial(
          "Disabled trial name", 1000000000, "Default group",
          FieldTrial::SESSION_RANDOMIZED, &default_group_number);
  disabled_trial->AppendGroup("Disabled trial group name", 999999999);
  disabled_trial->Disable();

  save_string.clear();
  FieldTrialList::AllStatesToString(&save_string, false);
  EXPECT_EQ("Some name/Default some name/*trial2/Winner/*xxx/yyyy/zzz/default/",
            save_string);
  save_string.clear();
  FieldTrialList::AllStatesToString(&save_string, true);
  EXPECT_EQ(
      "Disabled trial name/Default group/"
      "Some name/Default some name/*trial2/Winner/*xxx/yyyy/zzz/default/",
      save_string);
}

TEST_F(FieldTrialTest, Restore) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Some_name"));
  ASSERT_FALSE(FieldTrialList::TrialExists("xxx"));

  FieldTrialList::CreateTrialsFromString("Some_name/Winner/xxx/yyyy/");

  FieldTrial* trial = FieldTrialList::Find("Some_name");
  ASSERT_NE(static_cast<FieldTrial*>(nullptr), trial);
  EXPECT_EQ("Winner", trial->group_name());
  EXPECT_EQ("Some_name", trial->trial_name());

  trial = FieldTrialList::Find("xxx");
  ASSERT_NE(static_cast<FieldTrial*>(nullptr), trial);
  EXPECT_EQ("yyyy", trial->group_name());
  EXPECT_EQ("xxx", trial->trial_name());
}

TEST_F(FieldTrialTest, RestoreNotEndingWithSlash) {
  EXPECT_TRUE(FieldTrialList::CreateTrialsFromString("tname/gname"));

  FieldTrial* trial = FieldTrialList::Find("tname");
  ASSERT_NE(static_cast<FieldTrial*>(nullptr), trial);
  EXPECT_EQ("gname", trial->group_name());
  EXPECT_EQ("tname", trial->trial_name());
}

TEST_F(FieldTrialTest, BogusRestore) {
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("MissingSlash"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("MissingGroupName/"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("noname, only group/"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("/emptyname"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("*/emptyname"));
}

TEST_F(FieldTrialTest, DuplicateRestore) {
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial("Some name", 10, "Default", nullptr);
  trial->AppendGroup("Winner", 10);
  // Finalize the group selection by accessing the selected group.
  trial->group();
  std::string save_string;
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/", save_string);

  // It is OK if we redundantly specify a winner.
  EXPECT_TRUE(FieldTrialList::CreateTrialsFromString(save_string));

  // But it is an error to try to change to a different winner.
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("Some name/Loser/"));
}

TEST_F(FieldTrialTest, CreateTrialsFromStringNotActive) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Abc"));
  ASSERT_FALSE(FieldTrialList::TrialExists("Xyz"));
  ASSERT_TRUE(FieldTrialList::CreateTrialsFromString("Abc/def/Xyz/zyx/"));

  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  ASSERT_TRUE(active_groups.empty());

  // Check that the values still get returned and querying them activates them.
  EXPECT_EQ("def", FieldTrialList::FindFullName("Abc"));
  EXPECT_EQ("zyx", FieldTrialList::FindFullName("Xyz"));

  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  ASSERT_EQ(2U, active_groups.size());
  EXPECT_EQ("Abc", active_groups[0].trial_name);
  EXPECT_EQ("def", active_groups[0].group_name);
  EXPECT_EQ("Xyz", active_groups[1].trial_name);
  EXPECT_EQ("zyx", active_groups[1].group_name);
}

TEST_F(FieldTrialTest, CreateTrialsFromStringForceActivation) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Abc"));
  ASSERT_FALSE(FieldTrialList::TrialExists("def"));
  ASSERT_FALSE(FieldTrialList::TrialExists("Xyz"));
  ASSERT_TRUE(
      FieldTrialList::CreateTrialsFromString("*Abc/cba/def/fed/*Xyz/zyx/"));

  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  ASSERT_EQ(2U, active_groups.size());
  EXPECT_EQ("Abc", active_groups[0].trial_name);
  EXPECT_EQ("cba", active_groups[0].group_name);
  EXPECT_EQ("Xyz", active_groups[1].trial_name);
  EXPECT_EQ("zyx", active_groups[1].group_name);
}

TEST_F(FieldTrialTest, CreateTrialsFromStringNotActiveObserver) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Abc"));

  TestFieldTrialObserver observer;
  ASSERT_TRUE(FieldTrialList::CreateTrialsFromString("Abc/def/"));
  RunLoop().RunUntilIdle();
  // Observer shouldn't be notified.
  EXPECT_TRUE(observer.trial_name().empty());

  // Check that the values still get returned and querying them activates them.
  EXPECT_EQ("def", FieldTrialList::FindFullName("Abc"));

  EXPECT_EQ("Abc", observer.trial_name());
  EXPECT_EQ("def", observer.group_name());
}

TEST_F(FieldTrialTest, CreateFieldTrial) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Some_name"));

  FieldTrialList::CreateFieldTrial("Some_name", "Winner");

  FieldTrial* trial = FieldTrialList::Find("Some_name");
  ASSERT_NE(static_cast<FieldTrial*>(nullptr), trial);
  EXPECT_EQ("Winner", trial->group_name());
  EXPECT_EQ("Some_name", trial->trial_name());
}

TEST_F(FieldTrialTest, CreateFieldTrialIsNotActive) {
  const char kTrialName[] = "CreateFieldTrialIsActiveTrial";
  const char kWinnerGroup[] = "Winner";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));
  FieldTrialList::CreateFieldTrial(kTrialName, kWinnerGroup);

  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_TRUE(active_groups.empty());
}

TEST_F(FieldTrialTest, DuplicateFieldTrial) {
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial("Some_name", 10, "Default", nullptr);
  trial->AppendGroup("Winner", 10);

  // It is OK if we redundantly specify a winner.
  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Some_name", "Winner");
  EXPECT_TRUE(trial1 != nullptr);

  // But it is an error to try to change to a different winner.
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Some_name", "Loser");
  EXPECT_TRUE(trial2 == nullptr);
}

TEST_F(FieldTrialTest, DisableImmediately) {
  int default_group_number = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial("trial", 100, "default", &default_group_number);
  trial->Disable();
  ASSERT_EQ("default", trial->group_name());
  ASSERT_EQ(default_group_number, trial->group());
}

TEST_F(FieldTrialTest, DisableAfterInitialization) {
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial("trial", 100, "default", nullptr);
  trial->AppendGroup("non_default", 100);
  trial->Disable();
  ASSERT_EQ("default", trial->group_name());
}

TEST_F(FieldTrialTest, ForcedFieldTrials) {
  // Validate we keep the forced choice.
  FieldTrial* forced_trial = FieldTrialList::CreateFieldTrial("Use the",
                                                              "Force");
  EXPECT_STREQ("Force", forced_trial->group_name().c_str());

  int default_group_number = -1;
  scoped_refptr<FieldTrial> factory_trial =
      CreateFieldTrial("Use the", 1000, "default", &default_group_number);
  EXPECT_EQ(factory_trial.get(), forced_trial);

  int chosen_group = factory_trial->AppendGroup("Force", 100);
  EXPECT_EQ(chosen_group, factory_trial->group());
  int not_chosen_group = factory_trial->AppendGroup("Dark Side", 100);
  EXPECT_NE(chosen_group, not_chosen_group);

  // Since we didn't force the default group, we should not be returned the
  // chosen group as the default group.
  EXPECT_NE(default_group_number, chosen_group);
  int new_group = factory_trial->AppendGroup("Duck Tape", 800);
  EXPECT_NE(chosen_group, new_group);
  // The new group should not be the default group either.
  EXPECT_NE(default_group_number, new_group);
}

TEST_F(FieldTrialTest, ForcedFieldTrialsDefaultGroup) {
  // Forcing the default should use the proper group ID.
  FieldTrial* forced_trial = FieldTrialList::CreateFieldTrial("Trial Name",
                                                              "Default");
  int default_group_number = -1;
  scoped_refptr<FieldTrial> factory_trial =
      CreateFieldTrial("Trial Name", 1000, "Default", &default_group_number);
  EXPECT_EQ(forced_trial, factory_trial.get());

  int other_group = factory_trial->AppendGroup("Not Default", 100);
  EXPECT_STREQ("Default", factory_trial->group_name().c_str());
  EXPECT_EQ(default_group_number, factory_trial->group());
  EXPECT_NE(other_group, factory_trial->group());

  int new_other_group = factory_trial->AppendGroup("Not Default Either", 800);
  EXPECT_NE(new_other_group, factory_trial->group());
}

TEST_F(FieldTrialTest, SetForced) {
  // Start by setting a trial for which we ensure a winner...
  int default_group_number = -1;
  scoped_refptr<FieldTrial> forced_trial =
      CreateFieldTrial("Use the", 1, "default", &default_group_number);
  EXPECT_EQ(forced_trial, forced_trial);

  int forced_group = forced_trial->AppendGroup("Force", 1);
  EXPECT_EQ(forced_group, forced_trial->group());

  // Now force it.
  forced_trial->SetForced();

  // Now try to set it up differently as a hard coded registration would.
  scoped_refptr<FieldTrial> hard_coded_trial =
      CreateFieldTrial("Use the", 1, "default", &default_group_number);
  EXPECT_EQ(hard_coded_trial, forced_trial);

  int would_lose_group = hard_coded_trial->AppendGroup("Force", 0);
  EXPECT_EQ(forced_group, hard_coded_trial->group());
  EXPECT_EQ(forced_group, would_lose_group);

  // Same thing if we would have done it to win again.
  scoped_refptr<FieldTrial> other_hard_coded_trial =
      CreateFieldTrial("Use the", 1, "default", &default_group_number);
  EXPECT_EQ(other_hard_coded_trial, forced_trial);

  int would_win_group = other_hard_coded_trial->AppendGroup("Force", 1);
  EXPECT_EQ(forced_group, other_hard_coded_trial->group());
  EXPECT_EQ(forced_group, would_win_group);
}

TEST_F(FieldTrialTest, SetForcedDefaultOnly) {
  const char kTrialName[] = "SetForcedDefaultOnly";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  int default_group = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  trial->SetForced();

  trial = CreateFieldTrial(kTrialName, 100, kDefaultGroupName, nullptr);
  EXPECT_EQ(default_group, trial->group());
  EXPECT_EQ(kDefaultGroupName, trial->group_name());
}

TEST_F(FieldTrialTest, SetForcedDefaultWithExtraGroup) {
  const char kTrialName[] = "SetForcedDefaultWithExtraGroup";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  int default_group = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  trial->SetForced();

  trial = CreateFieldTrial(kTrialName, 100, kDefaultGroupName, nullptr);
  const int extra_group = trial->AppendGroup("Extra", 100);
  EXPECT_EQ(default_group, trial->group());
  EXPECT_NE(extra_group, trial->group());
  EXPECT_EQ(kDefaultGroupName, trial->group_name());
}

TEST_F(FieldTrialTest, SetForcedTurnFeatureOn) {
  const char kTrialName[] = "SetForcedTurnFeatureOn";
  const char kExtraGroupName[] = "Extra";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  // Simulate a server-side (forced) config that turns the feature on when the
  // original hard-coded config had it disabled.
  scoped_refptr<FieldTrial> forced_trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, nullptr);
  forced_trial->AppendGroup(kExtraGroupName, 100);
  forced_trial->SetForced();

  int default_group = -1;
  scoped_refptr<FieldTrial> client_trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  const int extra_group = client_trial->AppendGroup(kExtraGroupName, 0);
  EXPECT_NE(default_group, extra_group);

  EXPECT_FALSE(client_trial->group_reported_);
  EXPECT_EQ(extra_group, client_trial->group());
  EXPECT_TRUE(client_trial->group_reported_);
  EXPECT_EQ(kExtraGroupName, client_trial->group_name());
}

TEST_F(FieldTrialTest, SetForcedTurnFeatureOff) {
  const char kTrialName[] = "SetForcedTurnFeatureOff";
  const char kExtraGroupName[] = "Extra";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  // Simulate a server-side (forced) config that turns the feature off when the
  // original hard-coded config had it enabled.
  scoped_refptr<FieldTrial> forced_trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, nullptr);
  forced_trial->AppendGroup(kExtraGroupName, 0);
  forced_trial->SetForced();

  int default_group = -1;
  scoped_refptr<FieldTrial> client_trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  const int extra_group = client_trial->AppendGroup(kExtraGroupName, 100);
  EXPECT_NE(default_group, extra_group);

  EXPECT_FALSE(client_trial->group_reported_);
  EXPECT_EQ(default_group, client_trial->group());
  EXPECT_TRUE(client_trial->group_reported_);
  EXPECT_EQ(kDefaultGroupName, client_trial->group_name());
}

TEST_F(FieldTrialTest, SetForcedChangeDefault_Default) {
  const char kTrialName[] = "SetForcedDefaultGroupChange";
  const char kGroupAName[] = "A";
  const char kGroupBName[] = "B";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  // Simulate a server-side (forced) config that switches which group is default
  // and ensures that the non-forced code receives the correct group numbers.
  scoped_refptr<FieldTrial> forced_trial =
      CreateFieldTrial(kTrialName, 100, kGroupAName, nullptr);
  forced_trial->AppendGroup(kGroupBName, 100);
  forced_trial->SetForced();

  int default_group = -1;
  scoped_refptr<FieldTrial> client_trial =
      CreateFieldTrial(kTrialName, 100, kGroupBName, &default_group);
  const int extra_group = client_trial->AppendGroup(kGroupAName, 50);
  EXPECT_NE(default_group, extra_group);

  EXPECT_FALSE(client_trial->group_reported_);
  EXPECT_EQ(default_group, client_trial->group());
  EXPECT_TRUE(client_trial->group_reported_);
  EXPECT_EQ(kGroupBName, client_trial->group_name());
}

TEST_F(FieldTrialTest, SetForcedChangeDefault_NonDefault) {
  const char kTrialName[] = "SetForcedDefaultGroupChange";
  const char kGroupAName[] = "A";
  const char kGroupBName[] = "B";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  // Simulate a server-side (forced) config that switches which group is default
  // and ensures that the non-forced code receives the correct group numbers.
  scoped_refptr<FieldTrial> forced_trial =
      CreateFieldTrial(kTrialName, 100, kGroupAName, nullptr);
  forced_trial->AppendGroup(kGroupBName, 0);
  forced_trial->SetForced();

  int default_group = -1;
  scoped_refptr<FieldTrial> client_trial =
      CreateFieldTrial(kTrialName, 100, kGroupBName, &default_group);
  const int extra_group = client_trial->AppendGroup(kGroupAName, 50);
  EXPECT_NE(default_group, extra_group);

  EXPECT_FALSE(client_trial->group_reported_);
  EXPECT_EQ(extra_group, client_trial->group());
  EXPECT_TRUE(client_trial->group_reported_);
  EXPECT_EQ(kGroupAName, client_trial->group_name());
}

TEST_F(FieldTrialTest, Observe) {
  const char kTrialName[] = "TrialToObserve1";
  const char kSecondaryGroupName[] = "SecondaryGroup";

  TestFieldTrialObserver observer;
  int default_group = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  const int secondary_group = trial->AppendGroup(kSecondaryGroupName, 50);
  const int chosen_group = trial->group();
  EXPECT_TRUE(chosen_group == default_group || chosen_group == secondary_group);

  // The observer should be notified synchronously by the group() call.
  EXPECT_EQ(kTrialName, observer.trial_name());
  if (chosen_group == default_group)
    EXPECT_EQ(kDefaultGroupName, observer.group_name());
  else
    EXPECT_EQ(kSecondaryGroupName, observer.group_name());
}

// Verify that no hang occurs when a FieldTrial group is selected from a
// FieldTrialList::Observer::OnFieldTrialGroupFinalized() notification. If the
// FieldTrialList's lock is held when observers are notified, this test will
// hang due to reentrant lock acquisition when selecting the FieldTrial group.
TEST_F(FieldTrialTest, ObserveReentrancy) {
  const char kTrialName1[] = "TrialToObserve1";
  const char kTrialName2[] = "TrialToObserve2";

  int default_group_1 = -1;
  scoped_refptr<FieldTrial> trial_1 =
      CreateFieldTrial(kTrialName1, 100, kDefaultGroupName, &default_group_1);

  FieldTrialObserverAccessingGroup observer(trial_1);

  int default_group_2 = -1;
  scoped_refptr<FieldTrial> trial_2 =
      CreateFieldTrial(kTrialName2, 100, kDefaultGroupName, &default_group_2);

  // No group should be selected for |trial_1| yet.
  EXPECT_EQ(FieldTrial::kNotFinalized, trial_1->group_);

  // Force selection of a group for |trial_2|. This will notify |observer| which
  // will force the selection of a group for |trial_1|. This should not hang.
  trial_2->group();

  // The above call should have selected a group for |trial_1|.
  EXPECT_NE(FieldTrial::kNotFinalized, trial_1->group_);
}

TEST_F(FieldTrialTest, ObserveDisabled) {
  const char kTrialName[] = "TrialToObserve2";

  TestFieldTrialObserver observer;
  int default_group = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  trial->AppendGroup("A", 25);
  trial->AppendGroup("B", 25);
  trial->AppendGroup("C", 25);
  trial->Disable();

  // Observer shouldn't be notified of a disabled trial.
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.trial_name().empty());
  EXPECT_TRUE(observer.group_name().empty());

  // Observer shouldn't be notified even after a |group()| call.
  EXPECT_EQ(default_group, trial->group());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.trial_name().empty());
  EXPECT_TRUE(observer.group_name().empty());
}

TEST_F(FieldTrialTest, ObserveForcedDisabled) {
  const char kTrialName[] = "TrialToObserve3";

  TestFieldTrialObserver observer;
  int default_group = -1;
  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, &default_group);
  trial->AppendGroup("A", 25);
  trial->AppendGroup("B", 25);
  trial->AppendGroup("C", 25);
  trial->SetForced();
  trial->Disable();

  // Observer shouldn't be notified of a disabled trial, even when forced.
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.trial_name().empty());
  EXPECT_TRUE(observer.group_name().empty());

  // Observer shouldn't be notified even after a |group()| call.
  EXPECT_EQ(default_group, trial->group());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.trial_name().empty());
  EXPECT_TRUE(observer.group_name().empty());
}

TEST_F(FieldTrialTest, DisabledTrialNotActive) {
  const char kTrialName[] = "DisabledTrial";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, 100, kDefaultGroupName, nullptr);
  trial->AppendGroup("X", 50);
  trial->Disable();

  // Ensure the trial is not listed as active.
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_TRUE(active_groups.empty());

  // Ensure the trial is not listed in the |StatesToString()| result.
  std::string states;
  FieldTrialList::StatesToString(&states);
  EXPECT_TRUE(states.empty());
}

TEST_F(FieldTrialTest, NotDisabled) {
  const char kTrialName[] = "NotDisabled";
  const char kGroupName[] = "Group2";
  const int kProbability = 100;
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  scoped_refptr<FieldTrial> trial =
      CreateFieldTrial(kTrialName, kProbability, kDefaultGroupName, nullptr);
  trial->AppendGroup(kGroupName, kProbability);
  EXPECT_EQ(kGroupName, trial->group_name());
}

TEST_F(FieldTrialTest, FloatBoundariesGiveEqualGroupSizes) {
  const int kBucketCount = 100;

  // Try each boundary value |i / 100.0| as the entropy value.
  for (int i = 0; i < kBucketCount; ++i) {
    const double entropy = i / static_cast<double>(kBucketCount);

    scoped_refptr<FieldTrial> trial(
        new FieldTrial("test", kBucketCount, "default", entropy));
    for (int j = 0; j < kBucketCount; ++j)
      trial->AppendGroup(NumberToString(j), 1);

    EXPECT_EQ(NumberToString(i), trial->group_name());
  }
}

TEST_F(FieldTrialTest, DoesNotSurpassTotalProbability) {
  const double kEntropyValue = 1.0 - 1e-9;
  ASSERT_LT(kEntropyValue, 1.0);

  scoped_refptr<FieldTrial> trial(
      new FieldTrial("test", 2, "default", kEntropyValue));
  trial->AppendGroup("1", 1);
  trial->AppendGroup("2", 1);

  EXPECT_EQ("2", trial->group_name());
}

TEST_F(FieldTrialTest, CreateSimulatedFieldTrial) {
  const char kTrialName[] = "CreateSimulatedFieldTrial";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  // Different cases to test, e.g. default vs. non default group being chosen.
  struct {
    double entropy_value;
    const char* expected_group;
  } test_cases[] = {
    { 0.4, "A" },
    { 0.85, "B" },
    { 0.95, kDefaultGroupName },
  };

  for (auto& test_case : test_cases) {
    TestFieldTrialObserver observer;
    scoped_refptr<FieldTrial> trial(FieldTrial::CreateSimulatedFieldTrial(
        kTrialName, 100, kDefaultGroupName, test_case.entropy_value));
    trial->AppendGroup("A", 80);
    trial->AppendGroup("B", 10);
    EXPECT_EQ(test_case.expected_group, trial->group_name());

    // Field trial shouldn't have been registered with the list.
    EXPECT_FALSE(FieldTrialList::TrialExists(kTrialName));
    EXPECT_EQ(0u, FieldTrialList::GetFieldTrialCount());

    // Observer shouldn't have been notified.
    RunLoop().RunUntilIdle();
    EXPECT_TRUE(observer.trial_name().empty());

    // The trial shouldn't be in the active set of trials.
    FieldTrial::ActiveGroups active_groups;
    FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
    EXPECT_TRUE(active_groups.empty());

    // The trial shouldn't be listed in the |StatesToString()| result.
    std::string states;
    FieldTrialList::StatesToString(&states);
    EXPECT_TRUE(states.empty());
  }
}

TEST(FieldTrialTestWithoutList, StatesStringFormat) {
  std::string save_string;

  // The test suite instantiates a FieldTrialList but for the purpose of these
  // tests it's cleaner to start from scratch.
  test::ScopedFieldTrialListResetter trial_list_resetter_;

  // Scoping the first FieldTrialList, as we need another one to test the
  // importing function.
  {
    FieldTrialList field_trial_list(nullptr);
    scoped_refptr<FieldTrial> trial =
        CreateFieldTrial("Abc", 10, "Default some name", nullptr);
    trial->AppendGroup("cba", 10);
    trial->group();
    scoped_refptr<FieldTrial> trial2 =
        CreateFieldTrial("Xyz", 10, "Default xxx", nullptr);
    trial2->AppendGroup("zyx", 10);
    trial2->group();
    scoped_refptr<FieldTrial> trial3 =
        CreateFieldTrial("zzz", 10, "default", nullptr);

    FieldTrialList::AllStatesToString(&save_string, false);
  }

  // Starting with a new blank FieldTrialList.
  FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(field_trial_list.CreateTrialsFromString(save_string));

  FieldTrial::ActiveGroups active_groups;
  field_trial_list.GetActiveFieldTrialGroups(&active_groups);
  ASSERT_EQ(2U, active_groups.size());
  EXPECT_EQ("Abc", active_groups[0].trial_name);
  EXPECT_EQ("cba", active_groups[0].group_name);
  EXPECT_EQ("Xyz", active_groups[1].trial_name);
  EXPECT_EQ("zyx", active_groups[1].group_name);
  EXPECT_TRUE(field_trial_list.TrialExists("zzz"));
}

TEST(FieldTrialDeathTest, OneTimeRandomizedTrialWithoutFieldTrialList) {
  test::ScopedFieldTrialListResetter resetter;
  // Trying to instantiate a one-time randomized field trial before the
  // FieldTrialList is created should crash.
  EXPECT_DEATH_IF_SUPPORTED(
      FieldTrialList::FactoryGetFieldTrial(
          "OneTimeRandomizedTrialWithoutFieldTrialList", 100, kDefaultGroupName,
          FieldTrial::ONE_TIME_RANDOMIZED, nullptr),
      "");
}

class FieldTrialListTest : public ::testing::Test {
 public:
  FieldTrialListTest() = default;

 private:
  // The test suite instantiates a FieldTrialList but for the purpose of these
  // tests it's cleaner to start from scratch.
  test::ScopedFieldTrialListResetter trial_list_resetter_;
};

#if defined(OS_FUCHSIA)
// TODO(crbug.com/752368): This is flaky on Fuchsia.
#define MAYBE_TestCopyFieldTrialStateToFlags \
  DISABLED_TestCopyFieldTrialStateToFlags
#else
#define MAYBE_TestCopyFieldTrialStateToFlags TestCopyFieldTrialStateToFlags
#endif
TEST_F(FieldTrialListTest, MAYBE_TestCopyFieldTrialStateToFlags) {
  constexpr char kFieldTrialHandleSwitch[] = "test-field-trial-handle";
  constexpr char kEnableFeaturesSwitch[] = "test-enable-features";
  constexpr char kDisableFeaturesSwitch[] = "test-disable-features";

  FieldTrialList field_trial_list(std::make_unique<MockEntropyProvider>());

  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->InitializeFromCommandLine("A,B", "C");

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  feature_list->RegisterFieldTrialOverride(
      "MyFeature", FeatureList::OVERRIDE_ENABLE_FEATURE, trial);

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  FilePath test_file_path = FilePath(FILE_PATH_LITERAL("Program"));
  CommandLine command_line = CommandLine(test_file_path);

  FieldTrialList::CopyFieldTrialStateToFlags(
      kFieldTrialHandleSwitch, kEnableFeaturesSwitch, kDisableFeaturesSwitch,
      &command_line);
  EXPECT_TRUE(command_line.HasSwitch(kFieldTrialHandleSwitch));

  // Explictly specified enabled/disabled features should be specified.
  EXPECT_EQ("A,B", command_line.GetSwitchValueASCII(kEnableFeaturesSwitch));
  EXPECT_EQ("C", command_line.GetSwitchValueASCII(kDisableFeaturesSwitch));
}

TEST_F(FieldTrialListTest, InstantiateAllocator) {
  FieldTrialList field_trial_list(nullptr);

  FieldTrialList::CreateFieldTrial("Trial1", "Group1");

  FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded();
  const void* memory = field_trial_list.field_trial_allocator_->data();
  size_t used = field_trial_list.field_trial_allocator_->used();

  // Ensure that the function is idempotent.
  FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded();
  const void* new_memory = field_trial_list.field_trial_allocator_->data();
  size_t new_used = field_trial_list.field_trial_allocator_->used();
  EXPECT_EQ(memory, new_memory);
  EXPECT_EQ(used, new_used);
}

TEST_F(FieldTrialListTest, AddTrialsToAllocator) {
  std::string save_string;
  base::ReadOnlySharedMemoryRegion shm_region;

  // Scoping the first FieldTrialList, as we need another one to test that it
  // matches.
  {
    FieldTrialList field_trial_list1(nullptr);
    FieldTrialList::CreateFieldTrial("Trial1", "Group1");
    FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded();
    FieldTrialList::AllStatesToString(&save_string, false);
    shm_region = FieldTrialList::DuplicateFieldTrialSharedMemoryForTesting();
    ASSERT_TRUE(shm_region.IsValid());
  }

  FieldTrialList field_trial_list2(nullptr);
  // 4 KiB is enough to hold the trials only created for this test.
  base::ReadOnlySharedMemoryMapping shm_mapping = shm_region.MapAt(0, 4 << 10);
  ASSERT_TRUE(shm_mapping.IsValid());
  FieldTrialList::CreateTrialsFromSharedMemoryMapping(std::move(shm_mapping));
  std::string check_string;
  FieldTrialList::AllStatesToString(&check_string, false);
  EXPECT_EQ(save_string, check_string);
}

TEST_F(FieldTrialListTest, DoNotAddSimulatedFieldTrialsToAllocator) {
  constexpr char kTrialName[] = "trial";
  base::ReadOnlySharedMemoryRegion shm_region;
  {
    FieldTrialList field_trial_list1(nullptr);

    // Create a simulated trial and a real trial and call group() on them, which
    // should only add the real trial to the field trial allocator.
    FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded();

    // This shouldn't add to the allocator.
    scoped_refptr<FieldTrial> simulated_trial =
        FieldTrial::CreateSimulatedFieldTrial(kTrialName, 100, "Simulated",
                                              0.95);
    simulated_trial->group();

    // This should add to the allocator.
    FieldTrial* real_trial =
        FieldTrialList::CreateFieldTrial(kTrialName, "Real");
    real_trial->group();

    shm_region = FieldTrialList::DuplicateFieldTrialSharedMemoryForTesting();
    ASSERT_TRUE(shm_region.IsValid());
  }

  // Check that there's only one entry in the allocator.
  FieldTrialList field_trial_list2(nullptr);
  // 4 KiB is enough to hold the trials only created for this test.
  base::ReadOnlySharedMemoryMapping shm_mapping = shm_region.MapAt(0, 4 << 10);
  ASSERT_TRUE(shm_mapping.IsValid());
  FieldTrialList::CreateTrialsFromSharedMemoryMapping(std::move(shm_mapping));
  std::string check_string;
  FieldTrialList::AllStatesToString(&check_string, false);
  ASSERT_EQ(check_string.find("Simulated"), std::string::npos);
}

TEST_F(FieldTrialListTest, AssociateFieldTrialParams) {
  FieldTrialList field_trial_list(nullptr);

  std::string trial_name("Trial1");
  std::string group_name("Group1");

  // Create a field trial with some params.
  FieldTrialList::CreateFieldTrial(trial_name, group_name);
  std::map<std::string, std::string> params;
  params["key1"] = "value1";
  params["key2"] = "value2";
  FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      trial_name, group_name, params);
  FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded();

  // Clear all cached params from the associator.
  FieldTrialParamAssociator::GetInstance()->ClearAllCachedParamsForTesting();
  // Check that the params have been cleared from the cache.
  std::map<std::string, std::string> cached_params;
  FieldTrialParamAssociator::GetInstance()->GetFieldTrialParamsWithoutFallback(
      trial_name, group_name, &cached_params);
  EXPECT_EQ(0U, cached_params.size());

  // Check that we fetch the param from shared memory properly.
  std::map<std::string, std::string> new_params;
  GetFieldTrialParams(trial_name, &new_params);
  EXPECT_EQ("value1", new_params["key1"]);
  EXPECT_EQ("value2", new_params["key2"]);
  EXPECT_EQ(2U, new_params.size());
}

#if defined(OS_FUCHSIA)
// TODO(crbug.com/752368): This is flaky on Fuchsia.
#define MAYBE_ClearParamsFromSharedMemory DISABLED_ClearParamsFromSharedMemory
#else
#define MAYBE_ClearParamsFromSharedMemory ClearParamsFromSharedMemory
#endif
TEST_F(FieldTrialListTest, MAYBE_ClearParamsFromSharedMemory) {
  std::string trial_name("Trial1");
  std::string group_name("Group1");

  base::ReadOnlySharedMemoryRegion shm_region;
  {
    FieldTrialList field_trial_list1(nullptr);

    // Create a field trial with some params.
    FieldTrial* trial =
        FieldTrialList::CreateFieldTrial(trial_name, group_name);
    std::map<std::string, std::string> params;
    params["key1"] = "value1";
    params["key2"] = "value2";
    FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
        trial_name, group_name, params);
    FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded();

    // Clear all params from the associator AND shared memory. The allocated
    // segments should be different.
    FieldTrial::FieldTrialRef old_ref = trial->ref_;
    FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    FieldTrial::FieldTrialRef new_ref = trial->ref_;
    EXPECT_NE(old_ref, new_ref);

    // Check that there are no params associated with the field trial anymore.
    std::map<std::string, std::string> new_params;
    GetFieldTrialParams(trial_name, &new_params);
    EXPECT_EQ(0U, new_params.size());

    // Now duplicate the handle so we can easily check that the trial is still
    // in shared memory via AllStatesToString.
    shm_region = FieldTrialList::DuplicateFieldTrialSharedMemoryForTesting();
    ASSERT_TRUE(shm_region.IsValid());
  }

  // Check that we have the trial.
  FieldTrialList field_trial_list2(nullptr);
  // 4 KiB is enough to hold the trials only created for this test.
  base::ReadOnlySharedMemoryMapping shm_mapping = shm_region.MapAt(0, 4 << 10);
  ASSERT_TRUE(shm_mapping.IsValid());
  FieldTrialList::CreateTrialsFromSharedMemoryMapping(std::move(shm_mapping));
  std::string check_string;
  FieldTrialList::AllStatesToString(&check_string, false);
  EXPECT_EQ("*Trial1/Group1/", check_string);
}

TEST_F(FieldTrialListTest, DumpAndFetchFromSharedMemory) {
  std::string trial_name("Trial1");
  std::string group_name("Group1");

  // Create a field trial with some params.
  FieldTrialList field_trial_list(nullptr);
  FieldTrialList::CreateFieldTrial(trial_name, group_name);
  std::map<std::string, std::string> params;
  params["key1"] = "value1";
  params["key2"] = "value2";
  FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      trial_name, group_name, params);

  // 4 KiB is enough to hold the trials only created for this test.
  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  ASSERT_TRUE(shm.IsValid());
  // We _could_ use PersistentMemoryAllocator, this just has less params.
  WritableSharedPersistentMemoryAllocator allocator(std::move(shm.mapping), 1,
                                                    "");

  // Dump and subsequently retrieve the field trial to |allocator|.
  FieldTrialList::DumpAllFieldTrialsToPersistentAllocator(&allocator);
  std::vector<const FieldTrial::FieldTrialEntry*> entries =
      FieldTrialList::GetAllFieldTrialsFromPersistentAllocator(allocator);

  // Check that we have the entry we put in.
  EXPECT_EQ(1u, entries.size());
  const FieldTrial::FieldTrialEntry* entry = entries[0];

  // Check that the trial and group names match.
  StringPiece shm_trial_name;
  StringPiece shm_group_name;
  entry->GetTrialAndGroupName(&shm_trial_name, &shm_group_name);
  EXPECT_EQ(trial_name, shm_trial_name);
  EXPECT_EQ(group_name, shm_group_name);

  // Check that the params match.
  std::map<std::string, std::string> shm_params;
  entry->GetParams(&shm_params);
  EXPECT_EQ(2u, shm_params.size());
  EXPECT_EQ("value1", shm_params["key1"]);
  EXPECT_EQ("value2", shm_params["key2"]);
}

// Shared-memory distribution of FieldTrial to child process is not implemented
// on Fuchsia: http://crbug.com/752368.
#if !defined(OS_NACL) && !defined(OS_IOS) && !defined(OS_FUCHSIA)
MULTIPROCESS_TEST_MAIN(SerializeSharedMemoryRegionMetadata) {
  std::string serialized =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("field_trials");
  std::string guid_string =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("guid");

#if defined(OS_WIN) || defined(OS_MAC)
  base::ReadOnlySharedMemoryRegion deserialized =
      FieldTrialList::DeserializeSharedMemoryRegionMetadata(serialized);
#elif defined(OS_ANDROID)
  // Use the arbitrary fd value selected in the main process.
  // File descriptors are not remapped on Android. They have to be looked up in
  // the GlobalDescriptors table instead.
  int fd = base::GlobalDescriptors::GetInstance()->MaybeGet(42);
  CHECK_NE(fd, -1);
  base::ReadOnlySharedMemoryRegion deserialized =
      FieldTrialList::DeserializeSharedMemoryRegionMetadata(fd, serialized);
#else
  // Use the arbitrary fd value selected in the main process.
  base::ReadOnlySharedMemoryRegion deserialized =
      FieldTrialList::DeserializeSharedMemoryRegionMetadata(42, serialized);
#endif  // defined(OS_WIN) || defined(OS_MAC)
  CHECK(deserialized.IsValid());
  CHECK_EQ(deserialized.GetGUID().ToString(), guid_string);
  CHECK(!deserialized.GetGUID().is_empty());

  return 0;
}

TEST_F(FieldTrialListTest, SerializeSharedMemoryRegionMetadata) {
  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  ASSERT_TRUE(shm.IsValid());

  std::string serialized =
      FieldTrialList::SerializeSharedMemoryRegionMetadata(shm.region);

  LaunchOptions options;

#if defined(OS_WIN)
  options.handles_to_inherit.push_back(shm.region.GetPlatformHandle());
#elif defined(OS_MAC)
  options.mach_ports_for_rendezvous.insert(
      std::make_pair('fldt', MachRendezvousPort{shm.region.GetPlatformHandle(),
                                                MACH_MSG_TYPE_COPY_SEND}));
#elif defined(OS_POSIX)

#if defined(OS_ANDROID)
  int shm_fd = shm.region.GetPlatformHandle();
#else
  int shm_fd = shm.region.GetPlatformHandle().fd;
#endif  // defined(OS_ANDROID)

  // Pick an arbitrary FD number to use for the shmem FD in the child.
  options.fds_to_remap.emplace_back(std::make_pair(shm_fd, 42));
#endif  // defined(OS_POSIX)
  CommandLine cmd_line = GetMultiProcessTestChildBaseCommandLine();
  cmd_line.AppendSwitchASCII("field_trials", serialized);
  cmd_line.AppendSwitchASCII("guid", shm.region.GetGUID().ToString());

  Process process = SpawnMultiProcessTestChild(
      "SerializeSharedMemoryRegionMetadata", cmd_line, options);

  int exit_code;
  EXPECT_TRUE(WaitForMultiprocessTestChildExit(
      process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(0, exit_code);
}
#endif  // !defined(OS_NACL) && !defined(OS_IOS) && !defined(OS_FUCHSIA)

// Verify that the field trial shared memory handle is really read-only, and
// does not allow writable mappings. Test disabled on NaCl, Fuchsia, and Mac,
// which don't support/implement shared memory configuration. For Fuchsia, see
// crbug.com/752368
#if !defined(OS_NACL) && !defined(OS_FUCHSIA) && !defined(OS_MAC)
TEST_F(FieldTrialListTest, CheckReadOnlySharedMemoryRegion) {
  FieldTrialList field_trial_list(nullptr);
  FieldTrialList::CreateFieldTrial("Trial1", "Group1");

  FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded();

  base::ReadOnlySharedMemoryRegion region =
      FieldTrialList::DuplicateFieldTrialSharedMemoryForTesting();
  ASSERT_TRUE(region.IsValid());

  ASSERT_TRUE(CheckReadOnlyPlatformSharedMemoryRegionForTesting(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region))));
}
#endif  // !OS_NACL && !OS_FUCHSIA && !OS_MAC

TEST_F(FieldTrialTest, TestAllParamsToString) {
  std::string exptected_output = "t1.g1:p1/v1/p2/v2";

  // Create study with one group and two params.
  std::map<std::string, std::string> params;
  params["p1"] = "v1";
  params["p2"] = "v2";
  FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      "t1", "g1", params);
  EXPECT_EQ(
      "", FieldTrialList::AllParamsToString(false, &MockEscapeQueryParamValue));

  scoped_refptr<FieldTrial> trial1 =
      CreateFieldTrial("t1", 100, "Default", nullptr);
  trial1->AppendGroup("g1", 100);
  trial1->group();
  EXPECT_EQ(exptected_output, FieldTrialList::AllParamsToString(
                                  false, &MockEscapeQueryParamValue));

  // Create study with two groups and params that don't belog to the assigned
  // group. This should be in the output.
  FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      "t2", "g2", params);
  scoped_refptr<FieldTrial> trial2 =
      CreateFieldTrial("t2", 100, "Default", nullptr);
  trial2->AppendGroup("g1", 100);
  trial2->AppendGroup("g2", 0);
  trial2->group();
  EXPECT_EQ(exptected_output, FieldTrialList::AllParamsToString(
                                  false, &MockEscapeQueryParamValue));
}

}  // namespace base
