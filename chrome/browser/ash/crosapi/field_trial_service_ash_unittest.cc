// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"

#include "base/metrics/field_trial.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {
constexpr char kTrialName1[] = "trial1";
constexpr char kTrialName2[] = "trial2";
constexpr char kTrialName3[] = "trial3";
constexpr char kGroupName[] = "Default";

void CreateAndActivateFieldTrial(const std::string& trial_name,
                                 const std::string& group_name,
                                 bool is_overridden) {
  base::FieldTrialList::CreateFieldTrial(
      trial_name, group_name, /*is_low_anonymity=*/false, is_overridden)
      ->Activate();
}

void VerifyFieldTrial(const mojom::FieldTrialGroupInfoPtr& info,
                      const std::string& trial_name,
                      const std::string& group_name,
                      bool is_overridden) {
  EXPECT_EQ(std::tie(trial_name, group_name, is_overridden),
            std::tie(info->trial_name, info->group_name, info->is_overridden));
}

class TestFieldTrialObserver : public mojom::FieldTrialObserver {
 public:
  using OnActivateCallback = base::RepeatingCallback<void(
      const std::vector<mojom::FieldTrialGroupInfoPtr>&)>;

  mojo::PendingRemote<mojom::FieldTrialObserver> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void set_on_activate(OnActivateCallback callback) {
    callback_ = std::move(callback);
  }

  mojo::Receiver<mojom::FieldTrialObserver> receiver_{this};

 private:
  // mojom::FieldTrialObserver:
  void OnFieldTrialGroupActivated(
      std::vector<mojom::FieldTrialGroupInfoPtr> infos) override {
    if (callback_)
      callback_.Run(infos);
  }

  OnActivateCallback callback_;
};
}  // namespace

class FieldTrialServiceAshTest : public testing::Test {
 public:
  FieldTrialServiceAshTest() = default;
  FieldTrialServiceAshTest(const FieldTrialServiceAshTest&) = delete;
  FieldTrialServiceAshTest& operator=(const FieldTrialServiceAshTest&) = delete;
  ~FieldTrialServiceAshTest() override = default;

  void SetUp() override {
    scoped_features_.InitWithEmptyFeatureAndFieldTrialLists();
  }

 protected:
  TestFieldTrialObserver observer_;

 private:
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(FieldTrialServiceAshTest, SendInitialFieldTrialsAndUpdate) {
  CreateAndActivateFieldTrial(kTrialName1, kGroupName, /*is_overridden=*/true);
  CreateAndActivateFieldTrial(kTrialName2, kGroupName, /*is_overridden=*/false);

  // Send initial field trials.
  base::RunLoop run_loop1;
  observer_.set_on_activate(base::BindLambdaForTesting(
      [&](const std::vector<mojom::FieldTrialGroupInfoPtr>& infos) {
        ASSERT_EQ(2u, infos.size());
        VerifyFieldTrial(infos[0], kTrialName1, kGroupName,
                         /*is_overridden=*/true);
        VerifyFieldTrial(infos[1], kTrialName2, kGroupName,
                         /*is_overridden=*/false);
        run_loop1.Quit();
        // Test won't proceed until this is called.
      }));
  FieldTrialServiceAsh service;
  service.AddFieldTrialObserver(observer_.BindAndGetRemote());
  run_loop1.Run();

  // Send field trial updates.
  CreateAndActivateFieldTrial(kTrialName3, kGroupName, /*is_overridden=*/false);
  base::RunLoop run_loop2;
  observer_.set_on_activate(base::BindLambdaForTesting(
      [&](const std::vector<mojom::FieldTrialGroupInfoPtr>& infos) {
        EXPECT_EQ(1u, infos.size());
        VerifyFieldTrial(infos[0], kTrialName3, kGroupName,
                         /*is_overridden=*/false);
        run_loop2.Quit();
        // Test won't exit until this is called.
      }));
  run_loop2.Run();
}

TEST_F(FieldTrialServiceAshTest, SendEmptyInitialFieldTrial) {
  FieldTrialServiceAsh service;
  service.AddFieldTrialObserver(observer_.BindAndGetRemote());

  // Send empty payload even if no field trials are active.
  // This is to make it more convenient for browser tests.
  base::RunLoop run_loop;
  observer_.set_on_activate(base::BindLambdaForTesting(
      [&](const std::vector<mojom::FieldTrialGroupInfoPtr>& infos) {
        EXPECT_EQ(0u, infos.size());
        run_loop.Quit();
        // Test won't exit until this is called.
      }));
  run_loop.Run();
}

}  // namespace crosapi
