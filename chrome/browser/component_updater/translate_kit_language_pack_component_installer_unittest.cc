// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {

using ::testing::_;
using ::testing::Return;

constexpr char kFakeTranslateKitVersion[] = "0.0.1";

}  // namespace

class TranslateKitLanguagePackComponentTest : public ::testing::Test {
 public:
  TranslateKitLanguagePackComponentTest() = default;

  void SetUp() override {
    ASSERT_TRUE(fake_install_dir_.CreateUniqueTempDir());
    SetVersion(kFakeTranslateKitVersion);
    on_device_translation::RegisterLocalStatePrefs(pref_service_.registry());
    scoped_path_override_ = std::make_unique<base::ScopedPathOverride>(
        component_updater::DIR_COMPONENT_PREINSTALLED,
        fake_install_dir_.GetPath());
  }

  // Not Copyable.
  TranslateKitLanguagePackComponentTest(
      const TranslateKitLanguagePackComponentTest&) = delete;
  TranslateKitLanguagePackComponentTest& operator=(
      const TranslateKitLanguagePackComponentTest&) = delete;

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

  content::BrowserTaskEnvironment env_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::ScopedTempDir fake_install_dir_;
  base::Version fake_version_;
  base::DictValue fake_manifest_;
  std::unique_ptr<base::ScopedPathOverride> scoped_path_override_;
};

void CreateFakeInstallation(const base::FilePath& base_dir,
                            on_device_translation::LanguagePackKey lang_pack) {
  base::FilePath component_dir = base_dir.Append(
      on_device_translation::GetLanguagePackRelativeInstallDir().AppendASCII(
          on_device_translation::GetPackageInstallDirName(lang_pack)));

  CHECK(base::CreateDirectory(component_dir));

  static constexpr std::string_view kManifestData = R"({
    "name": "%s",
    "version": "%s",
    "min_env_version": "%s"
  })";
  for (const std::string& sub_dir :
       GetPackageInstallSubDirNamesForVerification(lang_pack)) {
    CHECK(base::CreateDirectory(component_dir.AppendASCII(sub_dir)));
  }
  CHECK(base::WriteFile(
      component_dir.AppendASCII("manifest.json"),
      base::StringPrintf(kManifestData.data(), "FakeInstallation", "0.0.1",
                         "0.0.1")));
}

TEST_F(TranslateKitLanguagePackComponentTest, ComponentRegistration) {
  auto service = std::make_unique<MockComponentUpdateService>();
  base::RunLoop run_loop;
  base::RunLoop on_ready_loop;
  EXPECT_CALL(*service, RegisterComponent(_)).WillOnce(Return(true));
  EXPECT_CALL(*service, GetComponentIDs());
  // This is needed for triggering the on_ready callback.
  CreateFakeInstallation(fake_install_dir_.GetPath(),
                         on_device_translation::LanguagePackKey::kEn_Es);
  RegisterTranslateKitLanguagePackComponent(
      service.get(), pref_service(),
      on_device_translation::LanguagePackKey::kEn_Es, run_loop.QuitClosure(),
      on_ready_loop.QuitClosure());
  run_loop.Run();
  on_ready_loop.Run();
}

TEST_F(TranslateKitLanguagePackComponentTest,
       ComponentRegistrationAlreadyRegistered) {
  auto service = std::make_unique<MockComponentUpdateService>();
  base::RunLoop run_loop;
  EXPECT_CALL(*service, GetComponentIDs())
      .WillOnce(testing::Return(
          std::vector<std::string>({crx_file::id_util::GenerateIdFromHash(
              on_device_translation::GetLanguagePackComponentConfig(
                  on_device_translation::LanguagePackKey::kEn_Es)
                  .public_key_sha)})));
  RegisterTranslateKitLanguagePackComponent(
      service.get(), pref_service(),
      on_device_translation::LanguagePackKey::kEn_Es,
      /*registered_callback=*/base::BindOnce([]() { NOTREACHED(); }),
      base::BindRepeating([]() { NOTREACHED(); }));
  env().RunUntilIdle();
}

TEST_F(TranslateKitLanguagePackComponentTest, VerifyInstallation) {
  TranslateKitLanguagePackComponentInstallerPolicy policy(
      pref_service(), on_device_translation::LanguagePackKey::kEn_Es,
      base::RepeatingClosure());

  // Verify that the installation is not valid if the sub-directories are
  // missing.
  EXPECT_FALSE(policy.VerifyInstallation(manifest(), install_dir()));
  ASSERT_TRUE(
      base::CreateDirectory(install_dir().AppendASCII("en_es_dictionary")));
  EXPECT_FALSE(policy.VerifyInstallation(manifest(), install_dir()));
  ASSERT_TRUE(base::CreateDirectory(install_dir().AppendASCII("en_es_nmt")));
  EXPECT_FALSE(policy.VerifyInstallation(manifest(), install_dir()));
  ASSERT_TRUE(base::CreateDirectory(install_dir().AppendASCII("es_en_nmt")));
  // Verify that the installation is valid if the sub-directories are present.
  EXPECT_TRUE(policy.VerifyInstallation(manifest(), install_dir()));
}

TEST_F(TranslateKitLanguagePackComponentTest, AutoDownloadFeatureDisabled) {
  auto service = std::make_unique<MockComponentUpdateService>();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      on_device_translation::kAutoDownloadTranslateLanguagePacks);

  EXPECT_CALL(*service, RegisterComponent(_)).Times(0);
  RegisterTranslateKitLanguagePackComponentsForAutoDownload(service.get(),
                                                            pref_service());
}

TEST_F(TranslateKitLanguagePackComponentTest, AutoDownloadFeatureEnabled) {
  auto service = std::make_unique<MockComponentUpdateService>();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      on_device_translation::kAutoDownloadTranslateLanguagePacks,
      {{"language_pairs", "en-es,en-ja"}});

  // Expect registration for en-es and en-ja.
  base::RunLoop run_loop;
  int call_count = 0;
  EXPECT_CALL(*service, RegisterComponent(_))
      .Times(2)
      .WillRepeatedly([&](const ComponentRegistration& component) {
        ++call_count;
        if (call_count == 2) {
          run_loop.Quit();
        }
        return true;
      });
  EXPECT_CALL(*service, GetComponentIDs()).Times(2);

  RegisterTranslateKitLanguagePackComponentsForAutoDownload(service.get(),
                                                            pref_service());
  run_loop.Run();
}

TEST_F(TranslateKitLanguagePackComponentTest, AutoDownloadInvalidPair) {
  auto service = std::make_unique<MockComponentUpdateService>();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      on_device_translation::kAutoDownloadTranslateLanguagePacks,
      {{"language_pairs", "en-es,invalid-pair,en"}});

  // Only en-es is valid.
  base::RunLoop run_loop;
  EXPECT_CALL(*service, RegisterComponent(_))
      .WillOnce([&](const ComponentRegistration& component) {
        run_loop.Quit();
        return true;
      });
  EXPECT_CALL(*service, GetComponentIDs()).Times(1);

  RegisterTranslateKitLanguagePackComponentsForAutoDownload(service.get(),
                                                            pref_service());
  run_loop.Run();
}

}  // namespace component_updater
