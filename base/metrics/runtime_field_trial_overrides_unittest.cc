// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/runtime_field_trial_overrides.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/types/pass_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
class VariationsService {
 public:
  static base::PassKey<VariationsService> CreatePassKeyForTesting() {
    return base::PassKey<VariationsService>();
  }
};
}  // namespace variations

namespace base {

namespace {

const RuntimeFieldTrialInfo* FindOverride(
    const flat_map<std::string, std::unique_ptr<RuntimeFieldTrialInfo>>&
        overrides,
    std::string_view trial_name) {
  auto it = overrides.find(trial_name);
  return it == overrides.end() ? nullptr : it->second.get();
}

}  // namespace

class RuntimeFieldTrialOverridesTest : public ::testing::Test {
 public:
  class MockObserver : public RuntimeFieldTrialOverrides::Observer {
   public:
    void OnRuntimeFieldTrialOverride(
        const RuntimeFieldTrialInfo& override_info,
        std::string_view previous_override_trial_name) override {
      last_trial_name = override_info.trial_name();
      last_group_name = override_info.group_name();
      last_overridden_trial = override_info.overridden_trial();
      last_previous_override_trial_name =
          std::string(previous_override_trial_name);
      call_count++;
    }

    std::string last_trial_name;
    std::string last_group_name;
    raw_ptr<const FieldTrial> last_overridden_trial = nullptr;
    std::string last_previous_override_trial_name;
    int call_count = 0;

    void OnRuntimeFieldTrialOverrideActivated(
        const RuntimeFieldTrialInfo& override_info) override {
      last_activated_trial_name = override_info.trial_name();
      activated_call_count++;
    }

    std::string last_activated_trial_name;
    int activated_call_count = 0;
  };

  void TearDown() override {
    RuntimeFieldTrialOverrides::GetInstance()->ResetForTesting();
  }
};

TEST_F(RuntimeFieldTrialOverridesTest, ApplyAndGetOverrides) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Trial", "Group", base::FieldTrialParams(), nullptr),
      ""));

  auto* override_info = FindOverride(overrides->GetRuntimeOverrides(), "Trial");
  ASSERT_TRUE(override_info != nullptr);
  EXPECT_EQ(override_info->trial_name(), "Trial");
  EXPECT_EQ(override_info->group_name(), "Group");
  EXPECT_EQ(override_info->overridden_trial(), nullptr);
}

TEST_F(RuntimeFieldTrialOverridesTest, ApplyWithPreviousOverride) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch50Pct", "Disabled50", base::FieldTrialParams(), trial),
      ""));
  auto* override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch50Pct");
  ASSERT_TRUE(override_info != nullptr);
  EXPECT_EQ(override_info->trial_name(), "Killswitch50Pct");
  EXPECT_EQ(override_info->group_name(), "Disabled50");
  EXPECT_EQ(override_info->overridden_trial(), trial);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch100Pct", "Disabled100", base::FieldTrialParams(), trial),
      "Killswitch50Pct"));

  // The previous override should be removed.
  EXPECT_FALSE(FindOverride(overrides->GetRuntimeOverrides(),
                            "Killswitch50Pct") != nullptr);
  EXPECT_EQ(overrides->GetRuntimeOverrides().size(), 1);

  override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch100Pct");
  ASSERT_TRUE(override_info != nullptr);
  EXPECT_EQ(override_info->trial_name(), "Killswitch100Pct");
  EXPECT_EQ(override_info->group_name(), "Disabled100");
  EXPECT_EQ(override_info->overridden_trial(), trial);
}

TEST_F(RuntimeFieldTrialOverridesTest,
       ApplyWithSameTrialNameAsPreviousOverride) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch", "Disabled50Pct", base::FieldTrialParams(), nullptr),
      ""));
  EXPECT_TRUE(FindOverride(overrides->GetRuntimeOverrides(), "Killswitch") !=
              nullptr);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch", "Disabled100Pct", base::FieldTrialParams(), nullptr),
      "Killswitch"));

  auto* override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch");
  ASSERT_TRUE(override_info != nullptr);
  EXPECT_EQ(override_info->trial_name(), "Killswitch");
  EXPECT_EQ(override_info->group_name(), "Disabled100Pct");
  EXPECT_EQ(override_info->overridden_trial(), nullptr);
  EXPECT_EQ(overrides->GetRuntimeOverrides().size(), 1);
}

TEST_F(RuntimeFieldTrialOverridesTest, ObserverNotification) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  MockObserver observer;
  overrides->AddObserver(&observer);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "ObsTrial1", "ObsGroup1", base::FieldTrialParams(), nullptr),
      ""));
  EXPECT_EQ(observer.call_count, 1);
  EXPECT_EQ(observer.last_trial_name, "ObsTrial1");
  EXPECT_EQ(observer.last_group_name, "ObsGroup1");
  EXPECT_EQ(observer.last_overridden_trial, nullptr);
  EXPECT_EQ(observer.last_previous_override_trial_name, "");

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "ObsTrial2", "ObsGroup2", base::FieldTrialParams(), nullptr),
      "ObsTrial1"));
  EXPECT_EQ(observer.call_count, 2);
  EXPECT_EQ(observer.last_trial_name, "ObsTrial2");
  EXPECT_EQ(observer.last_group_name, "ObsGroup2");
  EXPECT_EQ(observer.last_overridden_trial, nullptr);
  EXPECT_EQ(observer.last_previous_override_trial_name, "ObsTrial1");

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest,
       ApplyFailsWhenPreviousOverrideDoesNotExist) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  MockObserver observer;
  overrides->AddObserver(&observer);

  EXPECT_FALSE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch", "Disabled", base::FieldTrialParams(), nullptr),
      "NonExistentTrial"));

  EXPECT_TRUE(overrides->GetRuntimeOverrides().empty());
  EXPECT_EQ(observer.call_count, 0);

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest,
       ApplyFailsWhenPreviousOverrideHasDifferentOverriddenTrial) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  MockObserver observer;
  overrides->AddObserver(&observer);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch1", "Disabled1", base::FieldTrialParams(), trial1),
      ""));
  EXPECT_EQ(observer.call_count, 1);

  // Attempt to apply a replacement override but with a different overridden
  // trial.
  EXPECT_FALSE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch2", "Disabled2", base::FieldTrialParams(), trial2),
      "Killswitch1"));

  // The previous override should still be present, and the new one should not
  // be added.
  EXPECT_EQ(overrides->GetRuntimeOverrides().size(), 1);
  auto* override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch1");
  ASSERT_TRUE(override_info != nullptr);
  EXPECT_EQ(override_info->trial_name(), "Killswitch1");
  EXPECT_EQ(override_info->group_name(), "Disabled1");
  EXPECT_EQ(override_info->overridden_trial(), trial1);

  EXPECT_FALSE(FindOverride(overrides->GetRuntimeOverrides(), "Killswitch2") !=
               nullptr);
  EXPECT_EQ(observer.call_count, 1);

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest, ApplyFailsWhenTrialNameAlreadyExists) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  MockObserver observer;
  overrides->AddObserver(&observer);

  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch", "Disabled1", base::FieldTrialParams(), trial1),
      ""));
  EXPECT_EQ(observer.call_count, 1);

  // Attempt to apply another override with the same trial name but without
  // specifying it as the previous override (i.e. a collision).
  EXPECT_FALSE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch", "Disabled2", base::FieldTrialParams(), trial2),
      ""));

  // The existing override should remain unmodified.
  EXPECT_EQ(overrides->GetRuntimeOverrides().size(), 1);
  auto* override_info =
      FindOverride(overrides->GetRuntimeOverrides(), "Killswitch");
  ASSERT_TRUE(override_info != nullptr);
  EXPECT_EQ(override_info->trial_name(), "Killswitch");
  EXPECT_EQ(override_info->group_name(), "Disabled1");
  EXPECT_EQ(override_info->overridden_trial(), trial1);

  EXPECT_EQ(observer.call_count, 1);

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest, GetRuntimeOverride) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  // Initially, looking up a trial should return nullptr.
  EXPECT_FALSE(overrides->GetRuntimeOverride("Trial") != nullptr);

  // Apply override.
  FieldTrial* trial =
      FieldTrialList::CreateFieldTrial("OriginalTrial", "OriginalGroup");
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Trial", "Group", base::FieldTrialParams(), trial),
      ""));

  // It should now return the override info.
  auto* override_info = overrides->GetRuntimeOverride("Trial");
  ASSERT_TRUE(override_info != nullptr);
  EXPECT_EQ(override_info->trial_name(), "Trial");
  EXPECT_EQ(override_info->group_name(), "Group");
  EXPECT_EQ(override_info->overridden_trial(), trial);

  // Looking up a different trial name should still return nullptr.
  EXPECT_FALSE(overrides->GetRuntimeOverride("OtherTrial") != nullptr);
}

TEST_F(RuntimeFieldTrialOverridesTest, IsFieldTrialOverridden) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  // Initially, neither trial is overridden.
  EXPECT_FALSE(overrides->IsFieldTrialOverridden(*trial1));
  EXPECT_FALSE(overrides->IsFieldTrialOverridden(*trial2));

  // Apply override for trial1.
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "OverrideTrial1", "Group1", base::FieldTrialParams(), trial1),
      ""));

  // trial1 should be overridden, trial2 should not.
  EXPECT_TRUE(overrides->IsFieldTrialOverridden(*trial1));
  EXPECT_FALSE(overrides->IsFieldTrialOverridden(*trial2));
}

TEST_F(RuntimeFieldTrialOverridesTest, Activation) {
  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  MockObserver observer;
  overrides->AddObserver(&observer);

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("Trial", "Group");

  // Activating an unregistered RuntimeFieldTrialInfo marks it active locally
  // without notifying observers.
  RuntimeFieldTrialInfo unregistered_info("UnregisteredTrial",
                                          "UnregisteredGroup",
                                          base::FieldTrialParams(), trial);
  EXPECT_FALSE(unregistered_info.is_active());
  unregistered_info.Activate();
  EXPECT_TRUE(unregistered_info.is_active());
  EXPECT_EQ(observer.activated_call_count, 0);

  // Apply an override (starts inactive).
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "OverrideTrial", "OverrideGroup", base::FieldTrialParams(), trial),
      /*previous_override_trial_name=*/""));

  EXPECT_EQ(observer.call_count, 1);
  EXPECT_EQ(observer.activated_call_count, 0);
  EXPECT_FALSE(overrides->IsRuntimeOverrideActive("OverrideTrial"));

  auto* override_info = overrides->GetRuntimeOverride("OverrideTrial");
  ASSERT_TRUE(override_info != nullptr);
  EXPECT_FALSE(override_info->is_active());

  // Activating the trial marks it active and notifies observers.
  override_info->Activate();
  EXPECT_TRUE(override_info->is_active());
  EXPECT_TRUE(overrides->IsRuntimeOverrideActive("OverrideTrial"));
  EXPECT_EQ(observer.activated_call_count, 1);
  EXPECT_EQ(observer.last_activated_trial_name, "OverrideTrial");

  // Activating again is a no-op and doesn't re-notify observers.
  override_info->Activate();
  EXPECT_EQ(observer.activated_call_count, 1);

  overrides->RemoveObserver(&observer);
}

TEST_F(RuntimeFieldTrialOverridesTest, ReentrantObserverActivation) {
  class RecordingObserver : public RuntimeFieldTrialOverrides::Observer {
   public:
    RecordingObserver(std::string_view name, std::vector<std::string>* events)
        : name_(name), events_(events) {}

    void OnRuntimeFieldTrialOverride(
        const RuntimeFieldTrialInfo& override_info,
        std::string_view previous_override_trial_name) override {
      events_->push_back(std::string(name_) +
                         ":Override:" + override_info.trial_name());
    }

    void OnRuntimeFieldTrialOverrideActivated(
        const RuntimeFieldTrialInfo& override_info) override {
      events_->push_back(std::string(name_) +
                         ":Activated:" + override_info.trial_name());
    }

   private:
    std::string name_;
    raw_ptr<std::vector<std::string>> events_;
  };

  class ActivatingObserver : public RecordingObserver {
   public:
    using RecordingObserver::RecordingObserver;

    void OnRuntimeFieldTrialOverride(
        const RuntimeFieldTrialInfo& override_info,
        std::string_view previous_override_trial_name) override {
      RecordingObserver::OnRuntimeFieldTrialOverride(
          override_info, previous_override_trial_name);
      if (override_info.trial_name() == "OverrideTrial3") {
        // Activate the newly applied override itself from within
        // OnRuntimeFieldTrialOverride(). Its
        // OnRuntimeFieldTrialOverrideActivated notification should be deferred
        // until all observers have received OnRuntimeFieldTrialOverride().
        RuntimeFieldTrialInfo* self_info =
            RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
                "OverrideTrial3");
        ASSERT_NE(self_info, nullptr);
        self_info->Activate();

        // Also re-entrantly activate a different, already-registered runtime
        // field trial ("OverrideTrial1") from within
        // OnRuntimeFieldTrialOverride().
        RuntimeFieldTrialInfo* other_info =
            RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
                "OverrideTrial1");
        ASSERT_NE(other_info, nullptr);
        other_info->Activate();
      }
    }

    void OnRuntimeFieldTrialOverrideActivated(
        const RuntimeFieldTrialInfo& override_info) override {
      RecordingObserver::OnRuntimeFieldTrialOverrideActivated(override_info);
      // Re-entrantly activate another runtime field trial ("OverrideTrial2")
      // from within OnRuntimeFieldTrialOverrideActivated().
      if (override_info.trial_name() == "OverrideTrial1") {
        RuntimeFieldTrialInfo* other_info =
            RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
                "OverrideTrial2");
        ASSERT_NE(other_info, nullptr);
        other_info->Activate();
      }
    }
  };

  auto* overrides = RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Trial2", "Group2");
  FieldTrial* trial3 = FieldTrialList::CreateFieldTrial("Trial3", "Group3");

  // Register OverrideTrial1 and OverrideTrial2 first (both start inactive).
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "OverrideTrial1", "OverrideGroup1", base::FieldTrialParams(), trial1),
      /*previous_override_trial_name=*/""));
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "OverrideTrial2", "OverrideGroup2", base::FieldTrialParams(), trial2),
      /*previous_override_trial_name=*/""));
  EXPECT_FALSE(overrides->IsRuntimeOverrideActive("OverrideTrial1"));
  EXPECT_FALSE(overrides->IsRuntimeOverrideActive("OverrideTrial2"));

  std::vector<std::string> events;
  ActivatingObserver observer1("Observer1", &events);
  RecordingObserver observer2("Observer2", &events);
  overrides->AddObserver(&observer1);
  overrides->AddObserver(&observer2);

  // Applying OverrideTrial3 triggers OnRuntimeFieldTrialOverride, which
  // activates OverrideTrial3 (deferred until after the
  // OnRuntimeFieldTrialOverride loop) and OverrideTrial1 (whose
  // OnRuntimeFieldTrialOverrideActivated in turn activates OverrideTrial2).
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "OverrideTrial3", "OverrideGroup3", base::FieldTrialParams(), trial3),
      /*previous_override_trial_name=*/""));

  EXPECT_TRUE(overrides->IsRuntimeOverrideActive("OverrideTrial1"));
  EXPECT_TRUE(overrides->IsRuntimeOverrideActive("OverrideTrial2"));
  EXPECT_TRUE(overrides->IsRuntimeOverrideActive("OverrideTrial3"));
  EXPECT_THAT(
      events,
      ::testing::ElementsAre(
          // Observer1 receives OverrideTrial3's override notification and
          // activates OverrideTrial3 (deferred) and OverrideTrial1 (immediate).
          "Observer1:Override:OverrideTrial3",
          // OverrideTrial1's activation immediately notifies Observer1, which
          // re-entrantly activates OverrideTrial2.
          "Observer1:Activated:OverrideTrial1",
          // OverrideTrial2's nested activation notifies both observers.
          "Observer1:Activated:OverrideTrial2",
          "Observer2:Activated:OverrideTrial2",
          // Unwinding back to OverrideTrial1's activation notifies Observer2.
          "Observer2:Activated:OverrideTrial1",
          // Unwinding back to OverrideTrial3's override notification notifies
          // Observer2 before OverrideTrial3's activation notification fires.
          "Observer2:Override:OverrideTrial3",
          // Now that all observers have received OnRuntimeFieldTrialOverride
          // for OverrideTrial3, its deferred activation notification fires.
          "Observer1:Activated:OverrideTrial3",
          "Observer2:Activated:OverrideTrial3"));

  overrides->RemoveObserver(&observer2);
  overrides->RemoveObserver(&observer1);
}

}  // namespace base
