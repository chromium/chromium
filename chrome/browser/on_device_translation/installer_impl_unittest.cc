// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/installer_impl.h"

#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"
#include "chrome/browser/data_saver/data_saver.h"
#include "chrome/browser/on_device_translation/library_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/dbus/image_loader/fake_image_loader_client.h"
#endif

namespace on_device_translation {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::ReturnRef;

class MockOnDemandUpdater : public component_updater::OnDemandUpdater {
 public:
  MockOnDemandUpdater() = default;
  ~MockOnDemandUpdater() override = default;

  MOCK_METHOD(void,
              OnDemandUpdate,
              (const std::string&,
               component_updater::OnDemandUpdater::Priority,
               component_updater::Callback),
              (override));
};

// Tests for OnDeviceTranslationInstallerImpl.
class OnDeviceTranslationInstallerTest : public ::testing::Test {
 public:
  explicit OnDeviceTranslationInstallerTest(base::FieldTrialParams params = {})
      : default_feature_list_(std::move(params)) {}
  ~OnDeviceTranslationInstallerTest() override = default;

  // Disallow copy and assign.
  OnDeviceTranslationInstallerTest(const OnDeviceTranslationInstallerTest&) =
      delete;
  OnDeviceTranslationInstallerTest& operator=(
      const OnDeviceTranslationInstallerTest&) = delete;

  void SetUp() override {
    auto mock_cus = std::make_unique<
        testing::NiceMock<component_updater::MockComponentUpdateService>>();
    // We capture the installer, so its lifetime is extended from the
    // `RegisterTranslateKitComponent` function.
    EXPECT_CALL(*mock_cus, RegisterComponent)
        .WillRepeatedly(
            [&](const component_updater::ComponentRegistration& registration) {
              // "Steal" the reference to keep it alive
              captured_installer_ = registration.installer;
              return true;
            });
    EXPECT_CALL(*mock_cus, GetOnDemandUpdater())
        .WillRepeatedly(ReturnRef(mock_ondemand_updater_));
    EXPECT_CALL(*mock_cus, UnregisterComponent).WillRepeatedly(Return(true));
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(
        std::move(mock_cus));
    local_state_ = TestingBrowserProcess::GetGlobal()->local_state();

#if BUILDFLAG(IS_CHROMEOS)
    ash::ImageLoaderClient::InitializeFake();
    fake_image_loader_client_ =
        static_cast<ash::FakeImageLoaderClient*>(ash::ImageLoaderClient::Get());
#endif
    CHECK(install_dir_.CreateUniqueTempDir());
    CHECK(component_user_dir_.CreateUniqueTempDir());
    scoped_path_override_ = std::make_unique<base::ScopedPathOverride>(
        component_updater::DIR_COMPONENT_PREINSTALLED, install_dir_.GetPath());
    component_user_scoped_path_override_ =
        std::make_unique<base::ScopedPathOverride>(
            component_updater::DIR_COMPONENT_USER,
            component_user_dir_.GetPath());
    installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>();
  }

  void TearDown() override {
    // Tests that go offline must not leak that into later tests.
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
    data_saver::ResetIsDataSaverEnabledForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    fake_image_loader_client_ = nullptr;
    ash::ImageLoaderClient::Shutdown();
#endif
    scoped_path_override_.reset();
  }
#if BUILDFLAG(IS_CHROMEOS)
  ash::FakeImageLoaderClient& fake_image_loader_client() {
    return *fake_image_loader_client_;
  }
#endif

 protected:
  void CreateFakeLanguagePackInstallation(
      const base::FilePath& base_dir,
      base::span<const LanguagePackKey> lang_packs) {
    for (const LanguagePackKey lang_pack : lang_packs) {
      base::FilePath component_dir = base_dir.Append(
          on_device_translation::GetLanguagePackRelativeInstallDir()
              .AppendASCII(
                  on_device_translation::GetPackageInstallDirName(lang_pack)));

      CHECK(base::CreateDirectory(component_dir));

      for (const std::string& sub_dir :
           GetPackageInstallSubDirNamesForVerification(lang_pack)) {
        const base::FilePath sub_dir_path = component_dir.AppendASCII(sub_dir);
        ASSERT_TRUE(base::CreateDirectory(sub_dir_path));
        ASSERT_TRUE(
            base::WriteFile(sub_dir_path.AppendASCII("model.bin"), "data"));
      }
      CHECK(base::WriteFile(component_dir.AppendASCII("manifest.json"), R"({
        "name": "FakeInstallation",
        "version": "0.0.1",
        "min_env_version": "0.0.1"
  })"));
    }
  }

  void CreateFakeInstallation(const base::FilePath& base_dir) {
#if BUILDFLAG(IS_CHROMEOS)
    fake_image_loader_client_->SetMountPathForComponent("ChromeTranslateKit",
                                                        base_dir);
#endif
    base::FilePath component_dir =
        base_dir.Append(on_device_translation::GetBinaryRelativeInstallDir());
    CHECK(base::CreateDirectory(component_dir));
    CHECK(
        base::CreateDirectory(component_dir.AppendASCII("TranslateKitFiles")));
    base::FilePath linux_file = component_dir.Append(
        FILE_PATH_LITERAL("TranslateKitFiles/libtranslatekit.so"));
    base::FilePath win_file = component_dir.Append(
        FILE_PATH_LITERAL("TranslateKitFiles/libtranslatekit.dll"));
    base::FilePath chromeos_file =
        component_dir.Append(FILE_PATH_LITERAL("image.squash"));
    CHECK(base::WriteFile(linux_file, ""));
    CHECK(base::WriteFile(win_file, ""));
    CHECK(base::WriteFile(chromeos_file, ""));

    CHECK(base::WriteFile(component_dir.AppendASCII("manifest.json"),
                          R"({
      "name": "FakeInstalation",
      "version": "0.0.1",
      "min_env_version": "0.0.1"
    })"));
  }

#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<ash::FakeImageLoaderClient> fake_image_loader_client_ = nullptr;
#endif
  std::unique_ptr<OnDeviceTranslationInstaller> installer_;
  raw_ptr<PrefService> local_state_ = nullptr;
  testing::NiceMock<MockOnDemandUpdater> mock_ondemand_updater_;
  base::ScopedTempDir install_dir_;
  base::ScopedTempDir component_user_dir_;
  std::unique_ptr<base::ScopedPathOverride> scoped_path_override_;
  std::unique_ptr<base::ScopedPathOverride>
      component_user_scoped_path_override_;
  // Initialized before `task_environment_` so worker threads see the feature
  // state from startup.
  struct ScopedTranslationFeatureList {
    explicit ScopedTranslationFeatureList(base::FieldTrialParams params) {
      params.emplace("TranslationAPILanguagePackMinFreeDiskSpaceMB", "0");
      feature_list.InitAndEnableFeatureWithParameters(
          blink::features::kTranslationAPI, params);
    }
    base::test::ScopedFeatureList feature_list;
  };

  // This installer is captured so its lifetime is extended and we do not end up
  // with a dangling pointer to the policy installer.
  scoped_refptr<update_client::CrxInstaller> captured_installer_;
  ScopedTranslationFeatureList default_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

class MockObserver : public OnDeviceTranslationInstaller::Observer {
 public:
  MOCK_METHOD(void,
              OnLanguagePackInstalled,
              (LanguagePackKey lang_pack),
              (override));
  MOCK_METHOD(void,
              OnLanguagePackInstallationChanged,
              (const LanguagePackKey),
              (override));
  MOCK_METHOD(void, OnInstallationChanged, (), (override));
  MOCK_METHOD(void,
              OnLanguagePackProgress,
              (const LanguagePackKey, int),
              (override));
  MOCK_METHOD(void, OnLibraryInstallFailed, (InstallError), (override));
  MOCK_METHOD(void,
              OnLanguagePackInstallFailed,
              (LanguagePackKey, InstallError),
              (override));
};

class FakeObserver : public OnDeviceTranslationInstaller::Observer {
 public:
  void OnLanguagePackInstalled(LanguagePackKey lang_pack) override {
    --expected_number_of_observations_;
    if (run_loop_ && expected_number_of_observations_ <= 0) {
      run_loop_->Quit();
    }
  }
  void OnLanguagePackInstallationChanged(
      const LanguagePackKey lang_pack) override {}
  void OnInstallationChanged() override {}

  void WaitForNotification(int expected_number_of_observations = 1) {
    expected_number_of_observations_ = expected_number_of_observations;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  int expected_number_of_observations_ = 1;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Tests that the translate kit component can be registered only once.
TEST_F(OnDeviceTranslationInstallerTest, Init) {
  CreateFakeInstallation(install_dir_.GetPath());
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));

  base::RunLoop run_loop;
  OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(OnDeviceTranslationInstaller::GetInstance()->IsInit());
  EXPECT_THAT(OnDeviceTranslationInstaller::GetInstance()
                  ->GetLibraryPath()
                  .MaybeAsASCII(),
              testing::StartsWith(install_dir_.GetPath().MaybeAsASCII()));
}

TEST_F(OnDeviceTranslationInstallerTest, IsNotInitWhileDownloadPending) {
  base::RunLoop on_demand_loop;
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce([&on_demand_loop](const std::string&,
                                  component_updater::OnDemandUpdater::Priority,
                                  component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::NONE);
        on_demand_loop.Quit();
      });

  installer_->Init(base::BindRepeating([]() { NOTREACHED(); }));
  EXPECT_FALSE(installer_->IsInit());
  on_demand_loop.Run();
  EXPECT_FALSE(installer_->IsInit());
}

// Tests that the translate kit component can be registered only once.
TEST_F(OnDeviceTranslationInstallerTest, InitWithCli) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      kTranslateKitBinaryPath,
      base::FilePath(FILE_PATH_LITERAL("/path/to/libtranslatekit.so")));
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
  installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>();

  base::RunLoop run_loop;
  OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(OnDeviceTranslationInstaller::GetInstance()->IsInit());
}

// Tests that `Init()` accepts a null callback, before and after the library is
// installed.
TEST_F(OnDeviceTranslationInstallerTest, InitWithNullCallback) {
  CreateFakeInstallation(install_dir_.GetPath());
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));
  OnDeviceTranslationInstaller::GetInstance()->Init(base::RepeatingClosure());
  // The install finishes while only the null callback waits.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(OnDeviceTranslationInstaller::GetInstance()->IsInit());
  base::RunLoop run_loop;
  OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(OnDeviceTranslationInstaller::GetInstance()->IsInit());
  // A null callback for an installed library must not be run.
  OnDeviceTranslationInstaller::GetInstance()->Init(base::RepeatingClosure());
}

// Tests that `Init()` fails right away when the library can never be
// installed, instead of keeping the callback forever.
TEST_F(OnDeviceTranslationInstallerTest, InitFailsWhenUnsupported) {
  installer_.reset();
  installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>(
      std::make_unique<FixedPathLibrarySource>(base::FilePath()));
  ASSERT_FALSE(installer_->IsSupported());
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
  testing::NiceMock<MockObserver> observer;
  installer_->AddObserver(&observer);
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kFailed));
  base::MockCallback<base::RepeatingClosure> on_ready;
  EXPECT_CALL(on_ready, Run()).Times(0);
  installer_->Init(on_ready.Get());
  task_environment_.RunUntilIdle();
  installer_->RemoveObserver(&observer);
}

// Tests that a failed on-demand update of the library is reported.
TEST_F(OnDeviceTranslationInstallerTest, InitFailsWhenUpdateFails) {
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(
          update_client::Error::UPDATE_DISABLED));
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kFailed))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  base::MockCallback<base::RepeatingClosure> on_ready;
  EXPECT_CALL(on_ready, Run()).Times(0);
  OnDeviceTranslationInstaller::GetInstance()->Init(on_ready.Get());
  run_loop.Run();
  EXPECT_FALSE(OnDeviceTranslationInstaller::GetInstance()->IsInit());
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that a download error during an on-demand update of the library is
// reported, even though the update request itself succeeds.
TEST_F(OnDeviceTranslationInstallerTest, InitFailsWhenDownloadFails) {
  auto* mock_cus = static_cast<
      testing::NiceMock<component_updater::MockComponentUpdateService>*>(
      TestingBrowserProcess::GetGlobal()->component_updater());
  EXPECT_CALL(*mock_cus, GetComponentDetails)
      .WillRepeatedly(
          [](const std::string& id, component_updater::CrxUpdateItem* item) {
            item->id = id;
            item->state = update_client::ComponentState::kUpdateError;
            item->error_category = update_client::ErrorCategory::kDownload;
            return true;
          });
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kFailed))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  OnDeviceTranslationInstaller::GetInstance()->Init(base::DoNothing());
  run_loop.Run();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that a language pack download that runs out of disk space is reported.
TEST_F(OnDeviceTranslationInstallerTest, InstallLanguagePackFailsWhenDiskFull) {
  CreateFakeInstallation(install_dir_.GetPath());
  auto* mock_cus = static_cast<
      testing::NiceMock<component_updater::MockComponentUpdateService>*>(
      TestingBrowserProcess::GetGlobal()->component_updater());
  // Only the language pack fails. On ChromeOS the library mounts after its
  // update ends, so its state must not look like a failed download either.
  const std::string pack_id = crx_file::id_util::GenerateIdFromHash(
      GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja).public_key_sha);
  EXPECT_CALL(*mock_cus, GetComponentDetails)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_cus, GetComponentDetails(pack_id, _))
      .WillRepeatedly(
          [](const std::string& id, component_updater::CrxUpdateItem* item) {
            item->id = id;
            item->state = update_client::ComponentState::kUpdateError;
            item->error_category = update_client::ErrorCategory::kDownload;
            item->error_code =
                static_cast<int>(update_client::CrxDownloaderError::DISK_FULL);
            return true;
          });
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .Times(2)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<2>(update_client::Error::NONE));
  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();
  }

  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLanguagePackInstallFailed(LanguagePackKey::kEn_Ja,
                                          InstallError::kNotEnoughDiskSpace))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  run_loop.Run();
  // The language pack stays registered, so that a later request retries it.
  EXPECT_THAT(
      OnDeviceTranslationInstaller::GetInstance()->RegisteredLanguagePacks(),
      testing::ElementsAre(LanguagePackKey::kEn_Ja));
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that a language pack that is registered but not installed is updated
// on demand instead of registered again.
TEST_F(OnDeviceTranslationInstallerTest,
       InstallLanguagePackUpdatesRegisteredComponent) {
  CreateFakeInstallation(install_dir_.GetPath());
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));
  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();
  }

  const std::string component_id = crx_file::id_util::GenerateIdFromHash(
      GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja).public_key_sha);
  auto* mock_cus = static_cast<
      testing::NiceMock<component_updater::MockComponentUpdateService>*>(
      TestingBrowserProcess::GetGlobal()->component_updater());
  EXPECT_CALL(*mock_cus, GetComponentIDs())
      .WillRepeatedly(Return(std::vector<std::string>{component_id}));
  EXPECT_CALL(*mock_cus, RegisterComponent).Times(0);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(component_id, _, _))
      .WillOnce([&run_loop](const std::string&,
                            component_updater::OnDemandUpdater::Priority,
                            component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::NONE);
        run_loop.Quit();
      });
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  run_loop.Run();
}

// Tests that a language pack whose component is registered and installed
// doesn't start progress tracking, because nothing would stop it.
TEST_F(OnDeviceTranslationInstallerTest,
       InstallLanguagePackDoesNotTrackInstalledComponent) {
  CreateFakeInstallation(install_dir_.GetPath());
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));
  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();
  }

  const LanguagePackComponentConfig& config =
      GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja);
  local_state_->SetFilePath(GetComponentPathPrefName(config),
                            install_dir_.GetPath().AppendASCII("en_ja"));
  ASSERT_TRUE(OnDeviceTranslationInstaller::GetInstance()
                  ->InstalledLanguagePacks()
                  .contains(LanguagePackKey::kEn_Ja));

  auto* mock_cus = static_cast<
      testing::NiceMock<component_updater::MockComponentUpdateService>*>(
      TestingBrowserProcess::GetGlobal()->component_updater());
  // The install gets as far as the check for a registered component.
  EXPECT_CALL(*mock_cus, GetComponentIDs())
      .Times(testing::AtLeast(1))
      .WillRepeatedly(Return(std::vector<std::string>{
          crx_file::id_util::GenerateIdFromHash(config.public_key_sha)}));
  EXPECT_CALL(*mock_cus, RegisterComponent).Times(0);
  // The progress tracker observes the component updater.
  EXPECT_CALL(*mock_cus, AddObserver).Times(0);
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  task_environment_.RunUntilIdle();
}

// Tests that the library install fails right away when the device is offline,
// and concurrent `Init()` calls coalesce into a single failure notification.
TEST_F(OnDeviceTranslationInstallerTest, InitFailsWhenOffline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kOffline))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  OnDeviceTranslationInstaller::GetInstance()->Init(base::DoNothing());
  OnDeviceTranslationInstaller::GetInstance()->Init(base::DoNothing());
  run_loop.Run();
  task_environment_.RunUntilIdle();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that a language pack download doesn't start when the device is
// offline and doesn't emit the download histogram.
TEST_F(OnDeviceTranslationInstallerTest, InstallLanguagePackFailsWhenOffline) {
  base::HistogramTester histogram_tester;
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLanguagePackInstallFailed(LanguagePackKey::kEn_Ja,
                                                    InstallError::kOffline))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  run_loop.Run();
  EXPECT_TRUE(OnDeviceTranslationInstaller::GetInstance()
                  ->RegisteredLanguagePacks()
                  .empty());
  histogram_tester.ExpectTotalCount(
      "Translate.OnDeviceTranslation.Download.LanguagePair", 0);
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that `Init()` asks for the library again when an on-demand update ends
// without a result, for example because the periodic update check ran at the
// same time.
TEST_F(OnDeviceTranslationInstallerTest, InitRetriesAfterUpdateInProgress) {
  component_updater::Callback second_update_callback;
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(
          update_client::Error::UPDATE_IN_PROGRESS))
      .WillOnce([&second_update_callback](
                    const std::string&,
                    component_updater::OnDemandUpdater::Priority,
                    component_updater::Callback callback) {
        second_update_callback = std::move(callback);
      });
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  EXPECT_CALL(observer, OnLibraryInstallFailed).Times(0);
  OnDeviceTranslationInstaller::GetInstance()->Init(base::RepeatingClosure());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(OnDeviceTranslationInstaller::GetInstance()->IsInit());

  // The update ended without a result. The next `Init()` asks again.
  OnDeviceTranslationInstaller::GetInstance()->Init(base::RepeatingClosure());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(second_update_callback);

  // The new request reports its result.
  testing::Mock::VerifyAndClearExpectations(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kFailed))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  std::move(second_update_callback).Run(update_client::Error::UPDATE_DISABLED);
  run_loop.Run();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that a short offline moment doesn't fail a library install that runs.
TEST_F(OnDeviceTranslationInstallerTest, InitKeepsInstallThatRunsWhenOffline) {
  component_updater::Callback update_callback;
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce([&update_callback](const std::string&,
                                   component_updater::OnDemandUpdater::Priority,
                                   component_updater::Callback callback) {
        update_callback = std::move(callback);
      });
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kOffline))
      .Times(0);
  OnDeviceTranslationInstaller::GetInstance()->Init(base::RepeatingClosure());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(update_callback);

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  OnDeviceTranslationInstaller::GetInstance()->Init(base::RepeatingClosure());
  task_environment_.RunUntilIdle();

  // The install that runs reports its own result.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kFailed))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  std::move(update_callback).Run(update_client::Error::UPDATE_DISABLED);
  run_loop.Run();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that a short offline moment doesn't fail a language pack download that
// runs.
TEST_F(OnDeviceTranslationInstallerTest,
       InstallLanguagePackKeepsDownloadThatRunsWhenOffline) {
  CreateFakeInstallation(install_dir_.GetPath());
  component_updater::Callback pack_update_callback;
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE))
      .WillOnce(
          [&pack_update_callback](const std::string&,
                                  component_updater::OnDemandUpdater::Priority,
                                  component_updater::Callback callback) {
            pack_update_callback = std::move(callback);
          });
  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();
  }
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  EXPECT_CALL(observer, OnLanguagePackInstallFailed).Times(0);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(pack_update_callback);

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  task_environment_.RunUntilIdle();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that Data Saver blocks missing language pack downloads without starting
// an on-demand update.
TEST_F(OnDeviceTranslationInstallerTest,
       InstallLanguagePackFailsWhenDataSaverEnabled) {
  data_saver::OverrideIsDataSaverEnabledForTesting(true);
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLanguagePackInstallFailed(LanguagePackKey::kEn_Ja,
                                          InstallError::kDataSaverEnabled))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  run_loop.Run();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that the metered-network killswitch blocks library installs on metered
// connections without blocking metered connections by default.
TEST_F(OnDeviceTranslationInstallerTest, InitFailsWhenMeteredDownloadsBlocked) {
  net::test::ScopedMockNetworkChangeNotifier mock_network_change_notifier;
  mock_network_change_notifier.mock_network_change_notifier()
      ->SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kTranslationAPIBlockDownloadsOnMeteredNetwork);

  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLibraryInstallFailed(InstallError::kMeteredConnectionBlocked))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  OnDeviceTranslationInstaller::GetInstance()->Init(base::DoNothing());
  run_loop.Run();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that the metered-network killswitch blocks language pack downloads on
// metered connections and allows them when the feature is disabled.
TEST_F(OnDeviceTranslationInstallerTest,
       InstallLanguagePackRespectsMeteredKillswitch) {
  net::test::ScopedMockNetworkChangeNotifier mock_network_change_notifier;
  mock_network_change_notifier.mock_network_change_notifier()
      ->SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        kTranslationAPIBlockDownloadsOnMeteredNetwork);

    EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
    testing::NiceMock<MockObserver> observer;
    OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);

    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnLanguagePackInstallFailed(
                              LanguagePackKey::kEn_Ja,
                              InstallError::kMeteredConnectionBlocked))
        .WillOnce([&run_loop]() { run_loop.Quit(); });
    OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
        LanguagePackKey::kEn_Ja);
    run_loop.Run();
    EXPECT_TRUE(OnDeviceTranslationInstaller::GetInstance()
                    ->RegisteredLanguagePacks()
                    .empty());
    OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
  }

  // When disabled by default, metered downloads proceed.
  CreateFakeLanguagePackInstallation(install_dir_.GetPath(),
                                     {LanguagePackKey::kEn_Ja});
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));
  FakeObserver observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  observer.WaitForNotification();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that cancelling an in-flight download after switching to a metered
// connection maps CANCELLED and UPDATE_DISABLED to kMeteredConnectionBlocked
// when the killswitch is enabled, and to kFailed when unmetered.
TEST_F(OnDeviceTranslationInstallerTest,
       MidDownloadMeteredCancelMapsToMeteredError) {
  CreateFakeInstallation(install_dir_.GetPath());
  net::test::ScopedMockNetworkChangeNotifier mock_network_change_notifier;
  auto* mock_cus = static_cast<
      testing::NiceMock<component_updater::MockComponentUpdateService>*>(
      TestingBrowserProcess::GetGlobal()->component_updater());
  const std::string pack_id = crx_file::id_util::GenerateIdFromHash(
      GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja).public_key_sha);
  update_client::ServiceError current_error =
      update_client::ServiceError::CANCELLED;
  net::NetworkChangeNotifier::ConnectionCost cost_during_update =
      net::NetworkChangeNotifier::CONNECTION_COST_METERED;

  EXPECT_CALL(*mock_cus, GetComponentDetails)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_cus, GetComponentDetails(pack_id, _))
      .WillRepeatedly([&current_error](const std::string& id,
                                       component_updater::CrxUpdateItem* item) {
        item->id = id;
        item->state = update_client::ComponentState::kUpdateError;
        item->error_category = update_client::ErrorCategory::kService;
        item->error_code = static_cast<int>(current_error);
        return true;
      });
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(pack_id, _, _))
      .WillRepeatedly([&mock_network_change_notifier, &cost_during_update](
                          const std::string&,
                          component_updater::OnDemandUpdater::Priority,
                          component_updater::Callback callback) {
        mock_network_change_notifier.mock_network_change_notifier()
            ->SetConnectionCost(cost_during_update);
        std::move(callback).Run(update_client::Error::NONE);
      });
  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kTranslationAPIBlockDownloadsOnMeteredNetwork);

  for (const auto service_error : {
           update_client::ServiceError::CANCELLED,
           update_client::ServiceError::UPDATE_DISABLED,
       }) {
    current_error = service_error;
    cost_during_update = net::NetworkChangeNotifier::CONNECTION_COST_METERED;
    mock_network_change_notifier.mock_network_change_notifier()
        ->SetConnectionCost(
            net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
    testing::NiceMock<MockObserver> observer;
    OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnLanguagePackInstallFailed(
                              LanguagePackKey::kEn_Ja,
                              InstallError::kMeteredConnectionBlocked))
        .WillOnce([&run_loop]() { run_loop.Quit(); });
    OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
        LanguagePackKey::kEn_Ja);
    run_loop.Run();
    OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
  }

  // On an unmetered connection, ServiceError::CANCELLED still maps to kFailed.
  current_error = update_client::ServiceError::CANCELLED;
  cost_during_update = net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED;
  mock_network_change_notifier.mock_network_change_notifier()
      ->SetConnectionCost(
          net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLanguagePackInstallFailed(LanguagePackKey::kEn_Ja,
                                                    InstallError::kFailed))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  run_loop.Run();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

TEST_F(OnDeviceTranslationInstallerTest, InstallationChanged) {
  CreateFakeInstallation(install_dir_.GetPath());
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));

  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();
  }

  MockObserver mock_observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&mock_observer);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer, OnInstallationChanged).WillOnce([&run_loop]() {
      // Quit the loop when this mock method is called
      run_loop.Quit();
    });
    local_state_->SetFilePath(
        prefs::kTranslateKitBinaryPath,
        install_dir_.GetPath().AppendASCII("new_installation_dir"));
    run_loop.Run();
  }
}

TEST_F(OnDeviceTranslationInstallerTest, RemoveObserverWorks) {
  CreateFakeInstallation(install_dir_.GetPath());
  CreateFakeLanguagePackInstallation(install_dir_.GetPath(),
                                     {LanguagePackKey::kEn_Ja});
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .Times(2)
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));

  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    MockObserver mock_observer;
    OnDeviceTranslationInstaller::GetInstance()->AddObserver(&mock_observer);
    OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&mock_observer);
    FakeObserver observer;
    OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
    OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
        LanguagePackKey::kEn_Ja);
    observer.WaitForNotification(1);
    // The mock must never be called as it was removed from the observer list.
    EXPECT_CALL(mock_observer, OnLanguagePackInstalled).Times(0);
  }
}

TEST_F(OnDeviceTranslationInstallerTest, InstalledLanguagesWithCli) {
  base::test::ScopedCommandLine scoped_command_line;
  // Format: language, language, path
  // Testing En-Es and Fr-De packs however fr_de is not a supported language
  // pack key.
  std::string package_arg = "en,es,/path/to/en_es,fr,de,/path/to/fr_de";

  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "translate-kit-packages",
      package_arg);  // kTranslateKitPackagePaths string value
  installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>();

  EXPECT_THAT(
      OnDeviceTranslationInstaller::GetInstance()->InstalledLanguagePacks(),
      testing::UnorderedElementsAre(LanguagePackKey::kEn_Es));
}

TEST_F(OnDeviceTranslationInstallerTest, InstallLanguagesWithCli) {
  base::test::ScopedCommandLine scoped_command_line;
  // Format: language, language, path
  // Testing En-Es and Fr-De packs however fr_de is not a supported language
  // pack key.
  std::string package_arg = "en,es,/path/to/en_es,fr,de,/path/to/fr_de";

  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "translate-kit-packages",
      package_arg);  // kTranslateKitPackagePaths string value
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      kTranslateKitBinaryPath,
      base::FilePath(FILE_PATH_LITERAL("/path/to/libtranslatekit.so")));

  installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>();

  MockObserver observer;
  EXPECT_CALL(observer, OnLanguagePackInstalled).Times(1);
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Es);
  EXPECT_THAT(OnDeviceTranslationInstaller::GetInstance()
                  ->GetLanguagePackPath(LanguagePackKey::kEn_Es)
                  .MaybeAsASCII(),
              "/path/to/en_es");
}

TEST_F(OnDeviceTranslationInstallerTest, InstallLanguagePack) {
  CreateFakeInstallation(install_dir_.GetPath());
  CreateFakeLanguagePackInstallation(install_dir_.GetPath(),
                                     {LanguagePackKey::kEn_Ja});

  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .Times(2)
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));

  base::RunLoop run_loop;
  OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(OnDeviceTranslationInstaller::GetInstance()->IsInit());

  FakeObserver observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  observer.WaitForNotification();

  EXPECT_THAT(
      OnDeviceTranslationInstaller::GetInstance()->RegisteredLanguagePacks(),
      testing::ElementsAre(LanguagePackKey::kEn_Ja));
  EXPECT_THAT(
      OnDeviceTranslationInstaller::GetInstance()->InstalledLanguagePacks(),
      testing::ElementsAre(LanguagePackKey::kEn_Ja));
  EXPECT_THAT(OnDeviceTranslationInstaller::GetInstance()
                  ->GetLanguagePackPath(LanguagePackKey::kEn_Ja)
                  .MaybeAsASCII(),
              testing::StartsWith(install_dir_.GetPath()
                                      .AppendASCII("TranslateKit/models")
                                      .AppendASCII("en_ja")
                                      .MaybeAsASCII()));
}

TEST_F(OnDeviceTranslationInstallerTest, LanguagePackInstallationChanged) {
  CreateFakeInstallation(install_dir_.GetPath());
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .Times(2)
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));
  CreateFakeLanguagePackInstallation(install_dir_.GetPath(),
                                     {LanguagePackKey::kEn_Ja});
  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();

    OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
        LanguagePackKey::kEn_Ja);
  }

  testing::NiceMock<MockObserver> mock_observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&mock_observer);
  {
    base::RunLoop install_run_loop;
    EXPECT_CALL(mock_observer, OnLanguagePackInstalled(LanguagePackKey::kEn_Ja))
        .WillOnce([&install_run_loop]() { install_run_loop.Quit(); });
    install_run_loop.Run();
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer,
                OnLanguagePackInstallationChanged(LanguagePackKey::kEn_Ja))
        .WillOnce([&run_loop]() {
          // Quit the loop when this mock method is called
          run_loop.Quit();
        });
    local_state_->SetFilePath(
        GetComponentPathPrefName(
            *(kLanguagePackComponentConfigMap.at(LanguagePackKey::kEn_Ja))),
        install_dir_.GetPath().AppendASCII("new_installation_dir"));
    run_loop.Run();
  }
}

// Tests that progress tracking for a language pack whose component was
// registered earlier stops once the pack path pref is populated.
TEST_F(OnDeviceTranslationInstallerTest,
       StopsProgressTrackingOfRegisteredPackOnInstall) {
  CreateFakeInstallation(install_dir_.GetPath());
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));
  {
    base::RunLoop run_loop;
    OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
    run_loop.Run();
  }

  const LanguagePackComponentConfig& config =
      GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja);
  const std::string component_id =
      crx_file::id_util::GenerateIdFromHash(config.public_key_sha);
  auto* mock_cus = static_cast<
      testing::NiceMock<component_updater::MockComponentUpdateService>*>(
      TestingBrowserProcess::GetGlobal()->component_updater());
  EXPECT_CALL(*mock_cus, GetComponentIDs())
      .WillRepeatedly(Return(std::vector<std::string>{component_id}));
  EXPECT_CALL(*mock_cus, RegisterComponent).Times(0);

  component_updater::ComponentUpdateService::Observer* captured_observer =
      nullptr;
  bool observer_removed = false;
  EXPECT_CALL(*mock_cus, AddObserver)
      .WillOnce([&](component_updater::ComponentUpdateService::Observer*
                        cus_observer) { captured_observer = cus_observer; });
  EXPECT_CALL(*mock_cus, RemoveObserver)
      .WillRepeatedly([&](component_updater::ComponentUpdateService::Observer*
                              cus_observer) {
        if (cus_observer == captured_observer) {
          observer_removed = true;
        }
      });

  base::RunLoop on_demand_loop;
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(component_id, _, _))
      .WillOnce([&](const std::string&,
                    component_updater::OnDemandUpdater::Priority,
                    component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::NONE);
        on_demand_loop.Quit();
      });

  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  on_demand_loop.Run();
  ASSERT_TRUE(captured_observer);

  component_updater::CrxUpdateItem item;
  item.id = component_id;
  item.state = update_client::ComponentState::kDownloading;
  item.downloaded_bytes = 50;
  item.total_bytes = 100;
  base::RunLoop progress_loop;
  EXPECT_CALL(observer, OnLanguagePackProgress(LanguagePackKey::kEn_Ja, _))
      .WillOnce([&]() { progress_loop.Quit(); });
  captured_observer->OnEvent(item);
  progress_loop.Run();
  EXPECT_FALSE(observer_removed);

  local_state_->SetFilePath(GetComponentPathPrefName(config),
                            install_dir_.GetPath().AppendASCII("en_ja"));
  EXPECT_TRUE(observer_removed);
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

TEST_F(OnDeviceTranslationInstallerTest, MultipleInstallations) {
  CreateFakeInstallation(install_dir_.GetPath());
  CreateFakeLanguagePackInstallation(
      install_dir_.GetPath(),
      {LanguagePackKey::kEn_Ja, LanguagePackKey::kAr_En});

  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .Times(3)
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));

  base::RunLoop run_loop;
  OnDeviceTranslationInstaller::GetInstance()->Init(run_loop.QuitClosure());
  run_loop.Run();

  FakeObserver observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kAr_En);
  observer.WaitForNotification(2);
  EXPECT_THAT(
      OnDeviceTranslationInstaller::GetInstance()->RegisteredLanguagePacks(),
      testing::UnorderedElementsAre(LanguagePackKey::kEn_Ja,
                                    LanguagePackKey::kAr_En));
  EXPECT_THAT(
      OnDeviceTranslationInstaller::GetInstance()->InstalledLanguagePacks(),
      testing::UnorderedElementsAre(LanguagePackKey::kEn_Ja,
                                    LanguagePackKey::kAr_En));
}

// Tests that the translate kit language pack component can be registered and
// unregistered.
TEST_F(OnDeviceTranslationInstallerTest, RegisterAndUnInstallLanguagePack) {
  CreateFakeInstallation(install_dir_.GetPath());
  CreateFakeLanguagePackInstallation(install_dir_.GetPath(),
                                     {LanguagePackKey::kEn_Ja});
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .Times(2)
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE))
      .WillOnce(base::test::RunOnceCallback<2>(update_client::Error::NONE));

  base::RunLoop init_loop;
  OnDeviceTranslationInstaller::GetInstance()->Init(init_loop.QuitClosure());
  init_loop.Run();

  FakeObserver observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  observer.WaitForNotification();

  OnDeviceTranslationInstaller::GetInstance()->UnInstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  EXPECT_THAT(
      OnDeviceTranslationInstaller::GetInstance()->InstalledLanguagePacks(),
      testing::IsEmpty());
}

TEST_F(OnDeviceTranslationInstallerTest, ProgressReporting) {
  CreateFakeInstallation(install_dir_.GetPath());
  base::RunLoop on_demand_run_loop;
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate(_, _, _))
      .Times(2)  // Init and InstallLanguagePack
      .WillOnce([&](const std::string& id,
                    component_updater::OnDemandUpdater::Priority priority,
                    component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::NONE);
      })
      .WillOnce([&](const std::string& id,
                    component_updater::OnDemandUpdater::Priority priority,
                    component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::NONE);
        on_demand_run_loop.Quit();
      });
  component_updater::ComponentUpdateService::Observer* captured_observer =
      nullptr;
  auto* mock_cus = static_cast<
      testing::NiceMock<component_updater::MockComponentUpdateService>*>(
      TestingBrowserProcess::GetGlobal()->component_updater());
  EXPECT_CALL(*mock_cus, RemoveObserver(testing::_))
      .Times(testing::AnyNumber());

  EXPECT_CALL(*mock_cus, AddObserver)
      .WillRepeatedly(
          [&](component_updater::ComponentUpdateService::Observer* observer) {
            captured_observer = observer;
          });

  base::RunLoop init_loop;
  OnDeviceTranslationInstaller::GetInstance()->Init(init_loop.QuitClosure());
  init_loop.Run();

  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);

  on_demand_run_loop.Run();
  ASSERT_TRUE(captured_observer);

  component_updater::CrxUpdateItem item;
  const auto* config =
      kLanguagePackComponentConfigMap.at(LanguagePackKey::kEn_Ja);
  item.id = crx_file::id_util::GenerateIdFromHash(config->public_key_sha);
  item.state = update_client::ComponentState::kDownloading;
  item.downloaded_bytes = 50;
  item.total_bytes = 100;

  base::RunLoop run_loop_0;
  EXPECT_CALL(observer, OnLanguagePackProgress(LanguagePackKey::kEn_Ja, 0))
      .WillOnce([&]() { run_loop_0.Quit(); });

  captured_observer->OnEvent(item);

  run_loop_0.Run();

  item.downloaded_bytes = 100;

  base::RunLoop run_loop_100;
  EXPECT_CALL(observer, OnLanguagePackProgress(LanguagePackKey::kEn_Ja, 100))
      .WillOnce([&]() { run_loop_100.Quit(); });

  captured_observer->OnEvent(item);

  run_loop_100.Run();

  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

// Tests that a language pack download doesn't start when the disk is almost
// full.
TEST_F(OnDeviceTranslationInstallerTest,
       InstallLanguagePackFailsWithoutFreeDiskSpace) {
  base::test::ScopedFeatureList feature_list;
  // More free space than any test machine has.
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kTranslationAPI,
      {{"TranslationAPILanguagePackMinFreeDiskSpaceMB", "2000000000"}});
  EXPECT_CALL(mock_ondemand_updater_, OnDemandUpdate).Times(0);
  testing::NiceMock<MockObserver> observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLanguagePackInstallFailed(LanguagePackKey::kEn_Ja,
                                          InstallError::kNotEnoughDiskSpace))
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  run_loop.Run();
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(&observer);
}

}  // namespace
}  // namespace on_device_translation
