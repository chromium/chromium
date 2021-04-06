// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_component_installer.h"

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SodaComponentMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  SodaComponentMockComponentUpdateService() = default;
  ~SodaComponentMockComponentUpdateService() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SodaComponentMockComponentUpdateService);
};

}  // namespace

namespace component_updater {

class SodaComponentInstallerTest : public ::testing::Test {
 public:
  SodaComponentInstallerTest()
      : fake_install_dir_(FILE_PATH_LITERAL("base/install/dir/")),
        fake_version_("0.0.1") {}

  void SetUp() override {
    profile_prefs_.registry()->RegisterBooleanPref(prefs::kLiveCaptionEnabled,
                                                   false);
    local_state_.registry()->RegisterTimePref(prefs::kSodaScheduledDeletionTime,
                                              base::Time());
    profile_prefs_.registry()->RegisterStringPref(
        prefs::kLiveCaptionLanguageCode, "en-US");
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::FilePath fake_install_dir_;
  base::Version fake_version_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(SodaComponentInstallerTest,
       TestComponentRegistrationWhenLiveCaptionFeatureDisabled) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(media::kLiveCaption);
  std::unique_ptr<SodaComponentMockComponentUpdateService> component_updater(
      new SodaComponentMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_)).Times(0);
  RegisterSodaComponent(component_updater.get(), &profile_prefs_, &local_state_,
                        base::OnceClosure(), base::OnceClosure());
  task_environment_.RunUntilIdle();
}

TEST_F(SodaComponentInstallerTest,
       TestComponentRegistrationWhenUseSodaForLiveCaptionFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({media::kLiveCaption},
                                       {media::kUseSodaForLiveCaption});
  std::unique_ptr<SodaComponentMockComponentUpdateService> component_updater(
      new SodaComponentMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_)).Times(0);
  RegisterSodaComponent(component_updater.get(), &profile_prefs_, &local_state_,
                        base::OnceClosure(), base::OnceClosure());
  task_environment_.RunUntilIdle();
}

TEST_F(SodaComponentInstallerTest,
       TestComponentRegistrationWhenToggleDisabled) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitWithFeatures(
      {media::kUseSodaForLiveCaption, media::kLiveCaption}, {});
  profile_prefs_.SetBoolean(prefs::kLiveCaptionEnabled, false);
  std::unique_ptr<SodaComponentMockComponentUpdateService> component_updater(
      new SodaComponentMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_)).Times(0);
  RegisterSodaComponent(component_updater.get(), &profile_prefs_, &local_state_,
                        base::OnceClosure(), base::OnceClosure());
  task_environment_.RunUntilIdle();
}

TEST_F(SodaComponentInstallerTest,
       TestComponentRegistrationWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitWithFeatures(
      {media::kUseSodaForLiveCaption, media::kLiveCaption}, {});
  profile_prefs_.SetBoolean(prefs::kLiveCaptionEnabled, true);
  std::unique_ptr<SodaComponentMockComponentUpdateService> component_updater(
      new SodaComponentMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  RegisterSodaComponent(component_updater.get(), &profile_prefs_, &local_state_,
                        base::OnceClosure(), base::OnceClosure());
  task_environment_.RunUntilIdle();

  base::Time deletion_time =
      local_state_.GetTime(prefs::kSodaScheduledDeletionTime);
  ASSERT_EQ(base::Time(), deletion_time);
}

}  // namespace component_updater
