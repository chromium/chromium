// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_component_installer.h"

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/live_caption/pref_names.h"
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

  SodaComponentMockComponentUpdateService(
      const SodaComponentMockComponentUpdateService&) = delete;
  SodaComponentMockComponentUpdateService& operator=(
      const SodaComponentMockComponentUpdateService&) = delete;

  ~SodaComponentMockComponentUpdateService() override = default;
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
        prefs::kLiveCaptionLanguageCode, speech::kUsEnglishLocale);
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
  RegisterSodaComponent(component_updater.get(), &local_state_,
                        base::OnceClosure(), base::OnceClosure());
  task_environment_.RunUntilIdle();
}

}  // namespace component_updater
