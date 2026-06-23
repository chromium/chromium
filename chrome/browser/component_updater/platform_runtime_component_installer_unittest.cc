// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/platform_runtime_component_installer.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

using ::testing::_;

class MockOnDemandUpdater : public OnDemandUpdater {
 public:
  MOCK_METHOD3(OnDemandUpdate,
               void(const std::string& id,
                    Priority priority,
                    Callback callback));
};

class PlatformRuntimeComponentInstallerTest : public testing::Test {
 public:
  PlatformRuntimeComponentInstallerTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    TestingPrefServiceSimple* local_state =
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
    local_state->ClearPref(kPlatformRuntimeLastInstallTime);
    local_state->ClearPref(kPlatformRuntimeLastInstalledVersion);
  }

  base::ScopedTempDir component_install_dir_;
  content::BrowserTaskEnvironment env_;
};

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(PlatformRuntimeComponentInstallerTest, MaybeRegister_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnablePlatformRuntimeComponent);

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(0);
  MaybeRegisterPlatformRuntimeComponent(service.get());

  env_.RunUntilIdle();
}

TEST_F(PlatformRuntimeComponentInstallerTest,
       MaybeRegister_FeatureEnabled_NotInstalled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePlatformRuntimeComponent);

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  MockOnDemandUpdater mock_on_demand_updater;

  EXPECT_CALL(*service, RegisterComponent(_)).WillOnce(testing::Return(true));

  EXPECT_CALL(*service, GetComponentDetails(_, _))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(*service, GetOnDemandUpdater())
      .WillRepeatedly(testing::ReturnRef(mock_on_demand_updater));

  EXPECT_CALL(mock_on_demand_updater,
              OnDemandUpdate(_, OnDemandUpdater::Priority::FOREGROUND, _))
      .Times(1);

  MaybeRegisterPlatformRuntimeComponent(service.get());

  env_.RunUntilIdle();
}

TEST_F(PlatformRuntimeComponentInstallerTest,
       MaybeRegister_FeatureEnabled_Installed_NotStale) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePlatformRuntimeComponent);

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  MockOnDemandUpdater mock_on_demand_updater;

  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetTime(kPlatformRuntimeLastInstallTime, base::Time::Now());

  EXPECT_CALL(*service, RegisterComponent(_)).WillOnce(testing::Return(true));

  EXPECT_CALL(*service, GetComponentDetails(_, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = update_client::CrxComponent();
        item->component->version = base::Version("1.0.0.0");
        return true;
      });

  EXPECT_CALL(*service, GetOnDemandUpdater())
      .WillRepeatedly(testing::ReturnRef(mock_on_demand_updater));

  EXPECT_CALL(mock_on_demand_updater, OnDemandUpdate(_, _, _)).Times(0);

  MaybeRegisterPlatformRuntimeComponent(service.get());

  env_.RunUntilIdle();
}

TEST_F(PlatformRuntimeComponentInstallerTest,
       MaybeRegister_FeatureEnabled_Installed_Stale) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePlatformRuntimeComponent);

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  MockOnDemandUpdater mock_on_demand_updater;

  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetTime(kPlatformRuntimeLastInstallTime,
                       base::Time::Now() - base::Days(10));

  EXPECT_CALL(*service, RegisterComponent(_)).WillOnce(testing::Return(true));

  EXPECT_CALL(*service, GetComponentDetails(_, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = update_client::CrxComponent();
        item->component->version = base::Version("1.0.0.0");
        return true;
      });

  EXPECT_CALL(*service, GetOnDemandUpdater())
      .WillRepeatedly(testing::ReturnRef(mock_on_demand_updater));

  EXPECT_CALL(mock_on_demand_updater,
              OnDemandUpdate(_, OnDemandUpdater::Priority::FOREGROUND, _))
      .Times(1);

  MaybeRegisterPlatformRuntimeComponent(service.get());

  env_.RunUntilIdle();

  base::Time updated_time =
      local_state->GetTime(kPlatformRuntimeLastInstallTime);
  EXPECT_GT(updated_time, base::Time::Now() - base::Minutes(1));
}

#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(PlatformRuntimeComponentInstallerTest, ShouldTriggerInstallOrUpdate) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  const std::string crx_id = "test_crx_id";

  // GetComponentDetails returns false (not installed).
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce(testing::Return(false));
  EXPECT_TRUE(
      PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
          service.get(), local_state, crx_id));

  // GetComponentDetails returns true, but item component is not set.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = std::nullopt;
        return true;
      });
  EXPECT_TRUE(
      PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
          service.get(), local_state, crx_id));

  // GetComponentDetails returns true, component is set, but version is invalid.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = update_client::CrxComponent();
        item->component->version = base::Version();
        return true;
      });
  EXPECT_TRUE(
      PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
          service.get(), local_state, crx_id));

  // GetComponentDetails returns true, component version is kNullVersion.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = update_client::CrxComponent();
        item->component->version = base::Version("0.0.0.0");
        return true;
      });
  EXPECT_TRUE(
      PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
          service.get(), local_state, crx_id));

  // For the remaining cases, component is installed & valid.
  auto set_installed_mock = [&](MockComponentUpdateService* mock_service) {
    EXPECT_CALL(*mock_service, GetComponentDetails(crx_id, _))
        .WillOnce(
            [](const std::string& id, update_client::CrxUpdateItem* item) {
              item->component = update_client::CrxComponent();
              item->component->version = base::Version("1.0.0.0");
              return true;
            });
  };

  // Installed, valid version, but local_state is null.
  set_installed_mock(service.get());
  EXPECT_FALSE(
      PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
          service.get(), nullptr, crx_id));

  // Installed, valid version, local_state is valid but last install
  // time is null (not set).
  local_state->ClearPref(kPlatformRuntimeLastInstallTime);
  set_installed_mock(service.get());
  EXPECT_TRUE(
      PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
          service.get(), local_state, crx_id));

  // Installed, valid version, local_state has last install time > 7 days ago.
  local_state->SetTime(kPlatformRuntimeLastInstallTime,
                       base::Time::Now() - base::Days(8));
  set_installed_mock(service.get());
  EXPECT_TRUE(
      PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
          service.get(), local_state, crx_id));

  // Installed, valid version, local_state has last install time < 7 days ago.
  local_state->SetTime(kPlatformRuntimeLastInstallTime,
                       base::Time::Now() - base::Days(6));
  set_installed_mock(service.get());
  EXPECT_FALSE(
      PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
          service.get(), local_state, crx_id));
}

TEST_F(PlatformRuntimeComponentInstallerTest, ComponentReady_VersionChanged) {
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  local_state->SetString(kPlatformRuntimeLastInstalledVersion, "1.0.0.0");
  base::Time old_time = base::Time::Now() - base::Days(10);
  local_state->SetTime(kPlatformRuntimeLastInstallTime, old_time);

  PlatformRuntimeComponentInstallerPolicy policy;

  // Simulate a background update to a new version.
  policy.ComponentReadyForTesting(base::Version("1.0.0.1"),
                                  component_install_dir_.GetPath(),
                                  base::DictValue());

  // Verify that both the version and the install time are updated.
  EXPECT_EQ(local_state->GetString(kPlatformRuntimeLastInstalledVersion),
            "1.0.0.1");
  EXPECT_GT(local_state->GetTime(kPlatformRuntimeLastInstallTime), old_time);
}

TEST_F(PlatformRuntimeComponentInstallerTest, ComponentReady_VersionSame) {
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  local_state->SetString(kPlatformRuntimeLastInstalledVersion, "1.0.0.0");
  base::Time old_time = base::Time::Now() - base::Days(10);
  local_state->SetTime(kPlatformRuntimeLastInstallTime, old_time);

  PlatformRuntimeComponentInstallerPolicy policy;

  // Simulate startup or check where version hasn't changed.
  policy.ComponentReadyForTesting(base::Version("1.0.0.0"),
                                  component_install_dir_.GetPath(),
                                  base::DictValue());

  // Verify version is same and time is NOT updated.
  EXPECT_EQ(local_state->GetString(kPlatformRuntimeLastInstalledVersion),
            "1.0.0.0");
  EXPECT_EQ(local_state->GetTime(kPlatformRuntimeLastInstallTime), old_time);
}

TEST_F(PlatformRuntimeComponentInstallerTest, ComponentReady_VersionInvalid) {
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  local_state->SetString(kPlatformRuntimeLastInstalledVersion, "");
  base::Time old_time = base::Time::Now() - base::Days(10);
  local_state->SetTime(kPlatformRuntimeLastInstallTime, old_time);

  PlatformRuntimeComponentInstallerPolicy policy;

  base::Time current_time = base::Time::Now();
  // Simulate first install or bootstrapping.
  policy.ComponentReadyForTesting(base::Version("1.0.0.0"),
                                  component_install_dir_.GetPath(),
                                  base::DictValue());

  // Verify version is and time are recorded.
  EXPECT_EQ(local_state->GetString(kPlatformRuntimeLastInstalledVersion),
            "1.0.0.0");
  EXPECT_GT(local_state->GetTime(kPlatformRuntimeLastInstallTime),
            current_time);
}

}  // namespace component_updater
