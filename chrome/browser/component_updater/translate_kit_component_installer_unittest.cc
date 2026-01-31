// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_component_installer.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/dbus/image_loader/fake_image_loader_client.h"
#endif

namespace component_updater {
namespace {

using ::base::MockCallback;
using ::testing::_;
using ::testing::Return;

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: lbimbicckdokpoicboneldipejkhjgdg
constexpr uint8_t kTranslateKitPublicKeySHA256[32] = {
    0xb1, 0x8c, 0x18, 0x22, 0xa3, 0xea, 0xfe, 0x82, 0x1e, 0xd4, 0xb3,
    0x8f, 0x49, 0xa7, 0x96, 0x36, 0x55, 0xf3, 0xbc, 0x0d, 0xa5, 0x67,
    0x48, 0x09, 0xcd, 0x7b, 0xa9, 0x5f, 0xd8, 0x7f, 0x53, 0xb4};

constexpr char kFakeTranslateKitVersion[] = "0.0.1";

}  // namespace

class TranslateKitComponentMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  TranslateKitComponentMockComponentUpdateService() = default;

  // Not Copyable.
  TranslateKitComponentMockComponentUpdateService(
      const TranslateKitComponentMockComponentUpdateService&) = delete;
  TranslateKitComponentMockComponentUpdateService& operator=(
      const TranslateKitComponentMockComponentUpdateService&) = delete;
};

class RegisterTranslateKitComponentTest : public ::testing::Test {
 public:
  RegisterTranslateKitComponentTest() = default;

  void SetUp() override {
    ASSERT_TRUE(fake_install_dir_.CreateUniqueTempDir());
    SetVersion(kFakeTranslateKitVersion);
    on_device_translation::RegisterLocalStatePrefs(pref_service_.registry());
    scoped_path_override_ = std::make_unique<base::ScopedPathOverride>(
        component_updater::DIR_COMPONENT_PREINSTALLED,
        fake_install_dir_.GetPath());

#if BUILDFLAG(IS_CHROMEOS)
    ash::ImageLoaderClient::InitializeFake();
    fake_image_loader_client_ =
        static_cast<ash::FakeImageLoaderClient*>(ash::ImageLoaderClient::Get());
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS)
    fake_image_loader_client_ = nullptr;
    ash::ImageLoaderClient::Shutdown();
#endif
    scoped_path_override_.reset();
  }

  // Not Copyable.
  RegisterTranslateKitComponentTest(const RegisterTranslateKitComponentTest&) =
      delete;
  RegisterTranslateKitComponentTest& operator=(
      const RegisterTranslateKitComponentTest&) = delete;

 protected:
  content::BrowserTaskEnvironment& env() { return env_; }
  PrefService* pref_service() { return &pref_service_; }
  const base::FilePath& install_dir() const {
    return fake_install_dir_.GetPath();
  }
  const base::Version& version() const { return fake_version_; }
  const base::DictValue& manifest() const { return fake_manifest_; }

  void SetVersion(std::string_view version_str) {
    fake_version_ = base::Version(version_str);
    fake_manifest_.Set("version", version_str);
  }
#if BUILDFLAG(IS_CHROMEOS)
  ash::FakeImageLoaderClient& fake_image_loader_client() {
    return *fake_image_loader_client_;
  }
#endif

  void CreateFakeInstallation() {
#if BUILDFLAG(IS_CHROMEOS)
    fake_image_loader_client().SetMountPathForComponent("ChromeTranslateKit",
                                                        install_dir());
#endif
    base::FilePath component_dir = install_dir().Append(
        on_device_translation::GetBinaryRelativeInstallDir());
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
    "name": "FakeInstallation",
    "version": "0.0.1",
    "min_env_version": "0.0.1"
  })"));
  }

 private:
  content::BrowserTaskEnvironment env_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::ScopedTempDir fake_install_dir_;
  base::Version fake_version_;
  base::DictValue fake_manifest_;
  std::unique_ptr<base::ScopedPathOverride> scoped_path_override_;
#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<ash::FakeImageLoaderClient> fake_image_loader_client_ = nullptr;
#endif
};

TEST_F(RegisterTranslateKitComponentTest, ComponentRegistrationNoForceInstall) {
  auto service =
      std::make_unique<TranslateKitComponentMockComponentUpdateService>();

  RegisterTranslateKitComponent(
      service.get(), pref_service(),
      /*force_install=*/false,
      /*registered_callback=*/base::BindOnce([]() { NOTREACHED(); }),
      base::DoNothing());
  env().RunUntilIdle();
  EXPECT_FALSE(
      pref_service()->GetBoolean(prefs::kTranslateKitPreviouslyRegistered));
}

TEST_F(RegisterTranslateKitComponentTest,
       ComponentRegistrationNoForceInstallPreviouslyRegistered) {
  auto service =
      std::make_unique<TranslateKitComponentMockComponentUpdateService>();

  pref_service()->SetBoolean(prefs::kTranslateKitPreviouslyRegistered, true);
  EXPECT_CALL(*service, RegisterComponent(_));
  EXPECT_CALL(*service, GetComponentIDs());
  base::RunLoop run_loop;
  RegisterTranslateKitComponent(service.get(), pref_service(),
                                /*force_install=*/false,
                                /*registered_callback=*/run_loop.QuitClosure(),
                                base::DoNothing());
  run_loop.Run();
  EXPECT_TRUE(
      pref_service()->GetBoolean(prefs::kTranslateKitPreviouslyRegistered));
}

TEST_F(RegisterTranslateKitComponentTest, ComponentRegistrationForceInstall) {
  auto service =
      std::make_unique<TranslateKitComponentMockComponentUpdateService>();

  CreateFakeInstallation();
  // We capture the installer, so its lifetime is extended from the
  // `RegisterTranslateKitComponent` function.
  scoped_refptr<update_client::CrxInstaller> installer;
  EXPECT_CALL(*service, RegisterComponent)
      .WillOnce([&](const ComponentRegistration& registration) {
        // "Steal" the reference to keep it alive
        installer = registration.installer;
        return true;
      });

  EXPECT_CALL(*service, GetComponentIDs());
  base::RunLoop run_loop;
  base::RunLoop on_ready_loop;
  RegisterTranslateKitComponent(service.get(), pref_service(),
                                /*force_install=*/true,
                                /*registered_callback=*/run_loop.QuitClosure(),
                                /*on_ready_callback=*/
                                on_ready_loop.QuitClosure());
  run_loop.Run();
  on_ready_loop.Run();
  EXPECT_TRUE(
      pref_service()->GetBoolean(prefs::kTranslateKitPreviouslyRegistered));
}

TEST_F(RegisterTranslateKitComponentTest,
       ComponentRegistrationForceInstallAlreadyRegistered) {
  auto service =
      std::make_unique<TranslateKitComponentMockComponentUpdateService>();

  EXPECT_CALL(*service, GetComponentIDs())
      .WillOnce(testing::Return(
          std::vector<std::string>({crx_file::id_util::GenerateIdFromHash(
              kTranslateKitPublicKeySHA256)})));
  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run()).Times(0);

  RegisterTranslateKitComponent(service.get(), pref_service(),
                                /*force_install=*/true,
                                /*registered_callback=*/callback.Get(),
                                /*on_ready_callback=*/base::DoNothing());
  env().RunUntilIdle();
  EXPECT_FALSE(
      pref_service()->GetBoolean(prefs::kTranslateKitPreviouslyRegistered));
}

TEST_F(RegisterTranslateKitComponentTest, VerifyInstallationDefaultEmpty) {
  TranslateKitComponentInstallerPolicy policy(pref_service(),
                                              base::DoNothing());

  // An empty directory lacks all required files.
  EXPECT_FALSE(policy.VerifyInstallation(manifest(), install_dir()));
}

}  // namespace component_updater
