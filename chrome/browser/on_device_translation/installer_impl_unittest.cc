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
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

// The fake path for the update check.
constexpr std::string_view kFakeUpdateCheckPath = "/fake_update_check";

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
  OnDeviceTranslationInstallerTest() = default;
  ~OnDeviceTranslationInstallerTest() override = default;

  // Disallow copy and assign.
  OnDeviceTranslationInstallerTest(const OnDeviceTranslationInstallerTest&) =
      delete;
  OnDeviceTranslationInstallerTest& operator=(
      const OnDeviceTranslationInstallerTest&) = delete;

  void SetUp() override {
    auto mock_cus =
        std::make_unique<component_updater::MockComponentUpdateService>();
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
    // Set the component-updater check URL to the fake one.
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        "component-updater",
        base::StrCat({"url-source=", GURL(kFakeUpdateCheckPath).spec()}));
    installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>();
  }

  void TearDown() override {
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
        CHECK(base::CreateDirectory(component_dir.AppendASCII(sub_dir)));
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
  base::test::ScopedCommandLine command_line_;
  testing::NiceMock<MockOnDemandUpdater> mock_ondemand_updater_;
  base::ScopedTempDir install_dir_;
  base::ScopedTempDir component_user_dir_;
  std::unique_ptr<base::ScopedPathOverride> scoped_path_override_;
  std::unique_ptr<base::ScopedPathOverride>
      component_user_scoped_path_override_;
  // This installer is captured so its lifetime is extended and we do not end up
  // with a dangling pointer to the policy installer.
  scoped_refptr<update_client::CrxInstaller> captured_installer_;
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
              (const LanguagePackKey lang_pack),
              (override));
  MOCK_METHOD(void, OnInstallationChanged, (), (override));
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

  MockObserver mock_observer;
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

  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kEn_Ja);
  OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
      LanguagePackKey::kAr_En);
  FakeObserver observer;
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
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

}  // namespace
}  // namespace on_device_translation
