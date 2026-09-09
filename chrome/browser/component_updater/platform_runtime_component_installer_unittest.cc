// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/platform_runtime_component_installer.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#endif

namespace component_updater {

using ::testing::_;

class MockOnDemandUpdater : public OnDemandUpdater {
 public:
  MOCK_METHOD(void,
              OnDemandUpdate,
              (const std::string& id, Priority priority, Callback callback),
              (override));
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
    local_state->ClearPref(kPlatformRuntimeLastReleaseTime);
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
       MaybeRegister_FeatureEnabled_NotInstalled_DoesNotRegister) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePlatformRuntimeComponent);

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(0);
  MaybeRegisterPlatformRuntimeComponent(service.get());

  env_.RunUntilIdle();
}

#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(PlatformRuntimeComponentInstallerTest, ShouldTriggerInstallOrUpdate) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  const std::string crx_id = "test_crx_id";
  PlatformRuntimeComponentInstallerPolicy policy;

  // GetComponentDetails returns false (not installed).
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce(testing::Return(false));
  EXPECT_TRUE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));

  // GetComponentDetails returns true, but item component is not set.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = std::nullopt;
        return true;
      });
  EXPECT_TRUE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));

  // GetComponentDetails returns true, component is set, but version is invalid.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = update_client::CrxComponent();
        item->component->version = base::Version();
        return true;
      });
  EXPECT_TRUE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));

  // GetComponentDetails returns true, component version is kNullVersion.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = update_client::CrxComponent();
        item->component->version = base::Version("0.0.0.0");
        return true;
      });
  EXPECT_TRUE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));

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

  // Installed, valid version, local_state is valid but last install
  // time is null (not set).
  local_state->ClearPref(kPlatformRuntimeLastReleaseTime);
  set_installed_mock(service.get());
  EXPECT_TRUE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));

  // Installed, valid version, local_state has last install time > 7 days ago.
  local_state->SetTime(kPlatformRuntimeLastReleaseTime,
                       base::Time::Now() - base::Days(8));
  set_installed_mock(service.get());
  EXPECT_TRUE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));

  // Installed, valid version, local_state has last install time < 7 days ago.
  local_state->SetTime(kPlatformRuntimeLastReleaseTime,
                       base::Time::Now() - base::Days(6));
  set_installed_mock(service.get());
  EXPECT_FALSE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));
}

TEST_F(PlatformRuntimeComponentInstallerTest, ComponentReady_VersionChanged) {
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  local_state->SetString(kPlatformRuntimeLastInstalledVersion, "1.0.0.0");
  base::Time old_time = base::Time::Now() - base::Days(10);
  local_state->SetTime(kPlatformRuntimeLastReleaseTime, old_time);

  PlatformRuntimeComponentInstallerPolicy policy;
  base::HistogramTester histogram_tester;

  // Simulate a background update to a new version.
  policy.ComponentReadyForTesting(base::Version("1.0.0.1"),
                                  component_install_dir_.GetPath(),
                                  base::DictValue());

  // Verify that both the version and the install time are updated.
  EXPECT_EQ(local_state->GetString(kPlatformRuntimeLastInstalledVersion),
            "1.0.0.1");
  EXPECT_GT(local_state->GetTime(kPlatformRuntimeLastReleaseTime), old_time);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallTrigger",
      PlatformRuntimeInstallTrigger::kBackground, 1);
}

TEST_F(PlatformRuntimeComponentInstallerTest, ComponentReady_VersionSame) {
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  local_state->SetString(kPlatformRuntimeLastInstalledVersion, "1.0.0.0");
  base::Time old_time = base::Time::Now() - base::Days(10);
  local_state->SetTime(kPlatformRuntimeLastReleaseTime, old_time);

  PlatformRuntimeComponentInstallerPolicy policy;
  base::HistogramTester histogram_tester;

  // Simulate startup or check where version hasn't changed.
  policy.ComponentReadyForTesting(base::Version("1.0.0.0"),
                                  component_install_dir_.GetPath(),
                                  base::DictValue());

  // Verify version is same and time is NOT updated.
  EXPECT_EQ(local_state->GetString(kPlatformRuntimeLastInstalledVersion),
            "1.0.0.0");
  EXPECT_EQ(local_state->GetTime(kPlatformRuntimeLastReleaseTime), old_time);

  // Since version didn't change, no install should register.
  histogram_tester.ExpectTotalCount(
      "ComponentUpdater.PlatformRuntime.InstallTrigger", 0);
}

TEST_F(PlatformRuntimeComponentInstallerTest, ComponentReady_VersionInvalid) {
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  local_state->SetString(kPlatformRuntimeLastInstalledVersion, "");
  base::Time old_time = base::Time::Now() - base::Days(10);
  local_state->SetTime(kPlatformRuntimeLastReleaseTime, old_time);

  PlatformRuntimeComponentInstallerPolicy policy;
  policy.SetInstallTrigger(PlatformRuntimeInstallTrigger::kMissing);
  base::HistogramTester histogram_tester;

  base::Time current_time = base::Time::Now();
  // Simulate first install or bootstrapping.
  policy.ComponentReadyForTesting(base::Version("1.0.0.0"),
                                  component_install_dir_.GetPath(),
                                  base::DictValue());

  // Verify version is and time are recorded.
  EXPECT_EQ(local_state->GetString(kPlatformRuntimeLastInstalledVersion),
            "1.0.0.0");
  EXPECT_GT(local_state->GetTime(kPlatformRuntimeLastReleaseTime),
            current_time);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallTrigger",
      PlatformRuntimeInstallTrigger::kMissing, 1);
}

TEST_F(PlatformRuntimeComponentInstallerTest,
       ShouldTriggerInstallOrUpdate_SetsTrigger) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  const std::string crx_id = "test_crx_id";

  // Case: Missing trigger.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce(testing::Return(false));

  PlatformRuntimeComponentInstallerPolicy policy1;
  base::HistogramTester histogram_tester1;

  EXPECT_TRUE(
      policy1.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));

  // Triggering ComponentReady logs kMissing.
  policy1.ComponentReadyForTesting(base::Version("1.0.0.0"),
                                   component_install_dir_.GetPath(),
                                   base::DictValue());
  histogram_tester1.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallTrigger",
      PlatformRuntimeInstallTrigger::kMissing, 1);

  // Case: Stale trigger.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce([](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = update_client::CrxComponent();
        item->component->version = base::Version("1.0.0.0");
        return true;
      });
  local_state->SetTime(kPlatformRuntimeLastReleaseTime,
                       base::Time::Now() - base::Days(10));
  local_state->SetString(kPlatformRuntimeLastInstalledVersion, "1.0.0.0");

  PlatformRuntimeComponentInstallerPolicy policy2;
  base::HistogramTester histogram_tester2;

  EXPECT_TRUE(
      policy2.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));

  // Triggering ComponentReady with a new version logs kStale.
  policy2.ComponentReadyForTesting(base::Version("1.0.0.1"),
                                   component_install_dir_.GetPath(),
                                   base::DictValue());
  histogram_tester2.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallTrigger",
      PlatformRuntimeInstallTrigger::kStale, 1);
}

TEST_F(PlatformRuntimeComponentInstallerTest,
       ComponentReady_RecordsReleaseTimeFromVersionAndTriggersStale) {
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  // Test version with date <year>.<month>.<day>.<suffix> dynamically generated
  // 10 days in the past (`> 7 days` staleness threshold).
  base::Time past_time = base::Time::Now() - base::Days(10);
  base::Time::Exploded exploded;
  past_time.UTCExplode(&exploded);
  base::Version version(absl::StrFormat("%d.%d.%d.1", exploded.year,
                                        exploded.month, exploded.day_of_month));

  base::Time expected_release_time = past_time.UTCMidnight();

  PlatformRuntimeComponentInstallerPolicy policy;
  policy.ComponentReadyForTesting(version, component_install_dir_.GetPath(),
                                  base::DictValue());

  // Verify the preference stored the extracted UTC midnight date from the
  // version, not base::Time::Now().
  EXPECT_EQ(local_state->GetTime(kPlatformRuntimeLastReleaseTime),
            expected_release_time);

  // Verify that ShouldTriggerInstallOrUpdate immediately sees this version as
  // stale because its release time is older than 7 days threshold.
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  const std::string crx_id = "test_crx_id";
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce([&](const std::string& id, update_client::CrxUpdateItem* item) {
        item->component = update_client::CrxComponent();
        item->component->version = version;
        return true;
      });
  EXPECT_TRUE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));
}

#if BUILDFLAG(IS_WIN)

class MockAppCommand
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IAppCommandWeb> {
 public:
  // IDispatch methods.
  MOCK_METHOD(HRESULT,
              GetTypeInfoCount,
              (UINT * pctinfo),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetTypeInfo,
              (UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetIDsOfNames,
              (REFIID riid,
               LPOLESTR* rgszNames,
               UINT cNames,
               LCID lcid,
               DISPID* rgDispId),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              Invoke,
              (DISPID dispIdMember,
               REFIID riid,
               LCID lcid,
               WORD wFlags,
               DISPPARAMS* pDispParams,
               VARIANT* pVarResult,
               EXCEPINFO* pExcepInfo,
               UINT* puArgErr),
              (override, Calltype(STDMETHODCALLTYPE)));

  // IAppCommandWeb methods.
  MOCK_METHOD(HRESULT,
              get_status,
              (UINT*),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              get_exitCode,
              (DWORD*),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              get_output,
              (BSTR*),
              (override, Calltype(STDMETHODCALLTYPE)));

  MOCK_METHOD(HRESULT,
              execute,
              (VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT),
              (override, Calltype(STDMETHODCALLTYPE)));
};

class MockPlatformRuntimeInstallerDelegate
    : public PlatformRuntimeInstallerDelegate {
 public:
  MockPlatformRuntimeInstallerDelegate() = default;

  MOCK_METHOD((base::expected<Microsoft::WRL::ComPtr<IAppCommandWeb>, HRESULT>),
              GetAppCommand,
              (const std::wstring& command_name),
              (override));
  MOCK_METHOD(base::Process,
              LaunchProcess,
              (const base::CommandLine& cmd,
               const base::LaunchOptions& options),
              (override));
};

class PlatformRuntimeComponentInstallerWindowsTest
    : public PlatformRuntimeComponentInstallerTest {
 protected:
  void SetUp() override {
    PlatformRuntimeComponentInstallerTest::SetUp();
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    SetInstalled(true);
  }

  void SetInstalled(bool installed) {
    base::FilePath setup_dir = base::PathService::CheckedGet(base::DIR_EXE)
                                   .AppendASCII("1.0.0.0")
                                   .Append(installer::kInstallerDir);
    if (!installed) {
      base::DeletePathRecursively(
          base::PathService::CheckedGet(base::DIR_EXE).AppendASCII("1.0.0.0"));
    } else {
      base::CreateDirectory(setup_dir);
      base::WriteFile(setup_dir.Append(installer::kSetupExe), "dummy");
    }

    for (const HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
      if (!installed) {
        base::win::RegKey client_state_key(
            root, install_static::GetClientStateKeyPath().c_str(),
            KEY_SET_VALUE | KEY_WOW64_32KEY);
        client_state_key.DeleteValue(installer::kUninstallStringField);
        continue;
      }
      base::win::RegKey clients_key(root,
                                    install_static::GetClientsKeyPath().c_str(),
                                    KEY_SET_VALUE | KEY_WOW64_32KEY);
      clients_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0");

      base::win::RegKey client_state_key(
          root, install_static::GetClientStateKeyPath().c_str(),
          KEY_SET_VALUE | KEY_WOW64_32KEY);
      client_state_key.WriteValue(
          installer::kUninstallStringField,
          base::PathService::CheckedGet(base::DIR_EXE)
              .AppendASCII("1.0.0.0\\Installer\\setup.exe")
              .value()
              .c_str());
    }
  }

  base::ScopedPathOverride dir_exe_override_{base::DIR_EXE};
  base::ScopedPathOverride file_exe_override_{
      base::FILE_EXE, base::PathService::CheckedGet(base::DIR_EXE)
                          .Append(installer::kChromeExe)};
  registry_util::RegistryOverrideManager registry_override_manager_;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       MaybeRegister_FeatureEnabled_Installed_NotRegistered) {
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

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       MaybeRegister_FeatureEnabled_Installed_NotStale) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePlatformRuntimeComponent);

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  MockOnDemandUpdater mock_on_demand_updater;

  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetTime(kPlatformRuntimeLastReleaseTime, base::Time::Now());

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

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       MaybeRegister_FeatureEnabled_Installed_Stale) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePlatformRuntimeComponent);

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  MockOnDemandUpdater mock_on_demand_updater;

  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetTime(kPlatformRuntimeLastReleaseTime,
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
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_MissingInnerCrx_SystemLevel) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);
  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kInnerCrxNotFound, 1);
  histogram_tester.ExpectTotalCount(
      "ComponentUpdater.PlatformRuntime.InstallationResult.UserLevel", 0);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_MissingInnerCrx_UserLevel) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/false);
  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.UserLevel",
      PlatformRuntimeInstallationResult::kInnerCrxNotFound, 1);
  histogram_tester.ExpectTotalCount(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel", 0);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_SystemLevel_AppCommandNotFound) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(
          base::unexpected(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))));

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kCommandNotFound, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_SystemLevel_ExecuteAccessDenied) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(E_ACCESSDENIED));

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kFailedComAccessDenied, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_SystemLevel_GetStatusServerDied) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce(testing::Return(RPC_E_SERVER_DIED));

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kFailedComServerDied, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_SystemLevel_GetStatusOtherError) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce(testing::Return(E_FAIL));

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kFailedComOther, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_SystemLevel_CommandStatusError) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_ERROR;
        return S_OK;
      });

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kLaunchFailed, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_SystemLevel_Success) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_SUCCESS;
        return S_OK;
      });

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_EQ(result.result.code, 0);
  EXPECT_FALSE(base::PathExists(inner_crx));

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "ComponentUpdater.PlatformRuntime.InstallDuration.SystemLevel", 1);
  histogram_tester.ExpectTotalCount(
      "ComponentUpdater.PlatformRuntime.InstallDuration.UserLevel", 0);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_SystemLevel_SignatureFailure) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_FAILED_SIGNATURE;
        return S_OK;
      });

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kFailedSignature, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_SystemLevel_InternalFailure_Standard) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_FAILED_INTERNAL;
        return S_OK;
      });

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kFailedInternal, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_PerUser_CommandNotFound) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/false);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate, LaunchProcess(_, _))
      .WillOnce([](const base::CommandLine&, const base::LaunchOptions&) {
        ::SetLastError(ERROR_FILE_NOT_FOUND);
        return base::Process();
      });

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.UserLevel",
      PlatformRuntimeInstallationResult::kCommandNotFound, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_PerUser_LaunchFailed) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/false);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate, LaunchProcess(_, _))
      .WillOnce([](const base::CommandLine&, const base::LaunchOptions&) {
        ::SetLastError(ERROR_ACCESS_DENIED);
        return base::Process();
      });

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.UserLevel",
      PlatformRuntimeInstallationResult::kLaunchFailed, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_PerUser_InternalFailure) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/false);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate, LaunchProcess(_, _))
      .WillOnce(
          [](const base::CommandLine&, const base::LaunchOptions& options) {
            base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("cmd.exe")));
            cmd.AppendArg("/c");
            cmd.AppendArg(absl::StrFormat(
                "exit %d", installer::INSTALL_COMPONENT_FAILED_INTERNAL));
            return base::LaunchProcess(cmd, options);
          });

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);
  EXPECT_FALSE(base::PathExists(inner_crx));

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.UserLevel",
      PlatformRuntimeInstallationResult::kFailedInternal, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_PerUser_Success) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/false);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  auto mock_delegate = std::make_unique<
      testing::NiceMock<MockPlatformRuntimeInstallerDelegate>>();
  EXPECT_CALL(*mock_delegate, LaunchProcess(_, _))
      .WillOnce(
          [](const base::CommandLine&, const base::LaunchOptions& options) {
            base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("cmd.exe")));
            cmd.AppendArg("/c");
            cmd.AppendArg(absl::StrFormat(
                "exit %d", installer::INSTALL_COMPONENT_SUCCESS));
            return base::LaunchProcess(cmd, options);
          });

  PlatformRuntimeComponentInstallerPolicy policy(std::move(mock_delegate));
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_EQ(result.result.code, 0);
  EXPECT_FALSE(base::PathExists(inner_crx));

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.UserLevel",
      PlatformRuntimeInstallationResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "ComponentUpdater.PlatformRuntime.InstallDuration.UserLevel", 1);
  histogram_tester.ExpectTotalCount(
      "ComponentUpdater.PlatformRuntime.InstallDuration.SystemLevel", 0);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       VerifyInstallation_TargetApplicationDirectory) {
  base::FilePath expected_app_dir =
      base::PathService::CheckedGet(base::DIR_EXE);

  std::vector<uint8_t> hash;
  PlatformRuntimeComponentInstallerPolicy policy;
  policy.GetHash(&hash);
  std::string crx_id = crx_file::id_util::GenerateIdFromHash(hash);

  base::Version version("1.2.3.4");
  base::FilePath version_dir =
      expected_app_dir.AppendASCII(crx_id).AppendASCII(version.GetString());
  ASSERT_TRUE(base::CreateDirectory(version_dir));

  base::DictValue manifest;
  manifest.Set("version", version.GetString());

  // DLL and manifest missing -> returns false.
  EXPECT_FALSE(policy.VerifyInstallationForTesting(
      manifest, component_install_dir_.GetPath()));

  // Create manifest.json only -> returns false.
  ASSERT_TRUE(base::WriteFile(
      version_dir.Append(FILE_PATH_LITERAL("manifest.json")), "{}"));
  EXPECT_FALSE(policy.VerifyInstallationForTesting(
      manifest, component_install_dir_.GetPath()));

  // Create DLL -> returns true.
  ASSERT_TRUE(base::WriteFile(
      version_dir.AppendASCII(
          base::GetNativeLibraryName("chrome_platform_runtime")),
      "dummy dll"));
  EXPECT_TRUE(policy.VerifyInstallationForTesting(
      manifest, component_install_dir_.GetPath()));

  base::DeletePathRecursively(expected_app_dir.AppendASCII(crx_id));
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_NotInstalled_SystemLevel) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);
  SetInstalled(false);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  PlatformRuntimeComponentInstallerPolicy policy;
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);
  EXPECT_EQ(
      result.result.code,
      static_cast<int>(PlatformRuntimeInstallationResult::kCommandNotFound));
  EXPECT_FALSE(base::PathExists(inner_crx));

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.SystemLevel",
      PlatformRuntimeInstallationResult::kCommandNotFound, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       OnCustomInstall_NotInstalled_UserLevel) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/false);
  SetInstalled(false);

  base::FilePath inner_crx = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  ASSERT_TRUE(base::WriteFile(inner_crx, "dummy_crx_content"));

  PlatformRuntimeComponentInstallerPolicy policy;
  base::HistogramTester histogram_tester;

  auto result = policy.OnCustomInstallForTesting(
      base::DictValue(), component_install_dir_.GetPath());
  EXPECT_NE(result.result.code, 0);
  EXPECT_EQ(
      result.result.code,
      static_cast<int>(PlatformRuntimeInstallationResult::kCommandNotFound));
  EXPECT_FALSE(base::PathExists(inner_crx));

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.PlatformRuntime.InstallationResult.UserLevel",
      PlatformRuntimeInstallationResult::kCommandNotFound, 1);
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       VerifyInstallation_NotInstalled_ReturnsFalse) {
  SetInstalled(false);
  PlatformRuntimeComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallationForTesting(
      base::DictValue(), component_install_dir_.GetPath()));
}

TEST_F(
    PlatformRuntimeComponentInstallerWindowsTest,
    ShouldTriggerInstallOrUpdate_TriggersWhenLocalStateMissingEvenIfDiskExists) {
  base::ScopedTempDir fake_app_dir;
  ASSERT_TRUE(fake_app_dir.CreateUniqueTempDir());
  base::ScopedPathOverride path_override(base::DIR_LOCAL_APP_DATA,
                                         fake_app_dir.GetPath());

  base::FilePath expected_app_dir =
      installer::GetDefaultChromeInstallPath(install_static::IsSystemInstall());

  std::vector<uint8_t> hash;
  PlatformRuntimeComponentInstallerPolicy policy;
  policy.GetHash(&hash);
  std::string crx_id = crx_file::id_util::GenerateIdFromHash(hash);

  base::Time recent_time = base::Time::Now() - base::Days(2);
  base::Time::Exploded exploded;
  recent_time.UTCExplode(&exploded);
  base::Version recent_version(absl::StrFormat(
      "%d.%d.%d.1", exploded.year, exploded.month, exploded.day_of_month));

  base::FilePath version_dir = expected_app_dir.AppendASCII(crx_id).AppendASCII(
      recent_version.GetString());
  ASSERT_TRUE(base::CreateDirectory(version_dir));
  ASSERT_TRUE(base::WriteFile(
      version_dir.Append(FILE_PATH_LITERAL("manifest.json")), "{}"));

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  // Local state preferences and registered components are empty in this user's
  // profile.
  local_state->ClearPref(kPlatformRuntimeLastReleaseTime);
  local_state->ClearPref(kPlatformRuntimeLastInstalledVersion);

  // Even if disk contains an existing version (e.g. installed by another user),
  // ShouldTriggerInstallOrUpdate returns true to trigger an on-demand update
  // so this user's profile and local state are properly synchronized.
  EXPECT_CALL(*service, GetComponentDetails(crx_id, _))
      .WillOnce(testing::Return(false));
  EXPECT_TRUE(
      policy.ShouldTriggerInstallOrUpdate(service.get(), local_state, crx_id));
}

TEST_F(PlatformRuntimeComponentInstallerWindowsTest,
       ComponentReady_SyncsVersionToLocalState) {
  base::ScopedTempDir fake_app_dir;
  ASSERT_TRUE(fake_app_dir.CreateUniqueTempDir());
  base::ScopedPathOverride path_override(base::DIR_LOCAL_APP_DATA,
                                         fake_app_dir.GetPath());

  base::FilePath expected_app_dir =
      installer::GetDefaultChromeInstallPath(install_static::IsSystemInstall());

  std::vector<uint8_t> hash;
  PlatformRuntimeComponentInstallerPolicy policy;
  policy.GetHash(&hash);
  std::string crx_id = crx_file::id_util::GenerateIdFromHash(hash);

  base::Time recent_time = base::Time::Now() - base::Days(2);
  base::Time::Exploded exploded;
  recent_time.UTCExplode(&exploded);
  base::Version disk_version(absl::StrFormat(
      "%d.%d.%d.1", exploded.year, exploded.month, exploded.day_of_month));

  base::FilePath version_dir = expected_app_dir.AppendASCII(crx_id).AppendASCII(
      disk_version.GetString());
  ASSERT_TRUE(base::CreateDirectory(version_dir));
  ASSERT_TRUE(base::WriteFile(
      version_dir.Append(FILE_PATH_LITERAL("manifest.json")), "{}"));

  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->ClearPref(kPlatformRuntimeLastReleaseTime);
  local_state->ClearPref(kPlatformRuntimeLastInstalledVersion);

  base::Version update_client_version("2026.8.20.1");
  policy.ComponentReadyForTesting(update_client_version,
                                  component_install_dir_.GetPath(),
                                  base::DictValue());

  // Verify local_state was updated with the update_client version.
  EXPECT_EQ(local_state->GetString(kPlatformRuntimeLastInstalledVersion),
            update_client_version.GetString());
  base::Time expected_release_time;
  base::Time::Exploded expected_exploded = {
      .year = 2026,
      .month = 8,
      .day_of_month = 20,
  };
  EXPECT_TRUE(
      base::Time::FromUTCExploded(expected_exploded, &expected_release_time));
  EXPECT_EQ(local_state->GetTime(kPlatformRuntimeLastReleaseTime),
            expected_release_time);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace component_updater
