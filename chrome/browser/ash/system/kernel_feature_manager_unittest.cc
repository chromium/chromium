// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/kernel_feature_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class TestFakeDebugDaemonClient : public chromeos::FakeDebugDaemonClient {
 public:
  void GetKernelFeatureList(KernelFeatureListCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true,
                                  "TrialExample1,TrialExample2,TrialExample3"));
  }

  void KernelFeatureEnable(const std::string& name,
                           KernelFeatureListCallback callback) override {
    bool result = false;
    std::string out;

    // Hardcode the behavior of different trials.
    if (name == "TrialExample1") {
      result = true;
      out = "TrialExample1";
    } else if (name == "TrialExample2") {
      out = "Device does not support";
    } else if (name == "TrialExample3") {
      out = "Disable is the default, not doing anything";
    } else {
      out = "Unknown feature requested to be enabled";
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result, out));
  }
};

class KernelFeatureManagerTest : public testing::Test {
 public:
  KernelFeatureManagerTest() {
    // Provide an empty FeatureList to each test by default.
    scoped_feature_list_.InitWithFeatureList(
        std::make_unique<base::FeatureList>());
  }
  KernelFeatureManagerTest(const KernelFeatureManagerTest&) = delete;
  KernelFeatureManagerTest& operator=(const KernelFeatureManagerTest&) = delete;
  ~KernelFeatureManagerTest() override = default;

  TestFakeDebugDaemonClient debug_daemon_client_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(KernelFeatureManagerTest, EnableIfDeviceSupports) {
  base::test::ScopedFieldTrialListResetter resetter;
  base::FieldTrialList field_trial_list(nullptr);
  auto feature_list = std::make_unique<base::FeatureList>();

  // Feature that the device supports and we enable.
  base::FieldTrial* trial1 =
      base::FieldTrialList::CreateFieldTrial("TrialExample1", "A");

  // Feature that the device lists but does not support and we want to enable.
  base::FieldTrial* trial2 =
      base::FieldTrialList::CreateFieldTrial("TrialExample2", "B");

  // Feature that the device supports and we try to disable (note that disable
  // is the default so this really should not do anything. We test that
  // the code is not buggy enough to accidentally enable it.
  base::FieldTrial* trial3 =
      base::FieldTrialList::CreateFieldTrial("TrialExample3", "C");

  // Feature that the device does not list or support and we want to enable.
  base::FieldTrial* trial4 =
      base::FieldTrialList::CreateFieldTrial("TrialExample4", "D");

  feature_list->RegisterFieldTrialOverride(
      "TrialExample1", base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial1);
  feature_list->RegisterFieldTrialOverride(
      "TrialExample2", base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial2);
  feature_list->RegisterFieldTrialOverride(
      "TrialExample3", base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial3);
  feature_list->RegisterFieldTrialOverride(
      "TrialExample4", base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial4);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Initially, no trial should be active.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial1->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial2->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial3->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial4->trial_name()));

  KernelFeatureManager manager(&debug_daemon_client_);
  debug_daemon_client_.SetServiceIsAvailable(true);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial1->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial2->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial3->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial4->trial_name()));
}

}  // namespace
}  // namespace ash
