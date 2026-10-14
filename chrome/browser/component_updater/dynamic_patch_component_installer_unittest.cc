// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/dynamic_patch_component_installer.h"

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/child_module/features.h"
#include "chrome/browser/component_updater/install_product_component.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

using ::testing::_;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

using InstallCallback = DynamicPatchComponentInstallerPolicy::InstallCallback;
using Result = update_client::CrxInstaller::Result;

// The ID of the component, which is derived from the key that signs it.
constexpr std::string_view kDynamicPatchCrxId =
    "binbghhnflgglfabhjocbobkiignbgfh";

// The name of the inner CRX within the component.
constexpr base::FilePath::StringViewType kInnerCrxFileName =
    FILE_PATH_LITERAL("chrome_dynamic_patch.crx3");

constexpr std::string_view kPatchVersion = "1.2.3.4";

// A browser version other than that of the running browser.
constexpr std::string_view kOtherBaseVersion = "1.0.0.0";

// Returns a callback that fails the test if it is run.
InstallCallback UnexpectedInstall() {
  return base::BindRepeating([](const base::FilePath& inner_crx_path,
                                const base::FilePath& user_data_dir) {
    ADD_FAILURE() << "Unexpected installation of " << inner_crx_path;
    return ProductComponentInstallResult::kOtherInstallerError;
  });
}

// Returns a callback that returns `result` without installing anything.
InstallCallback ReturnResult(ProductComponentInstallResult result) {
  return base::BindRepeating(
      [](ProductComponentInstallResult result, const base::FilePath&,
         const base::FilePath&) { return result; },
      result);
}

// Returns a manifest for version `version` of the component.
base::DictValue MakeManifest(std::string_view version) {
  base::DictValue manifest;
  manifest.Set("name", "Chrome Dynamic Patch");
  manifest.Set("version", version);
  return manifest;
}

// Writes the files that setup.exe installs for a patch into `patch_dir`.
void StagePatch(const base::FilePath& patch_dir) {
  ASSERT_TRUE(base::CreateDirectory(patch_dir));
  ASSERT_TRUE(base::WriteFile(patch_dir.Append(chrome::kRendererDll), "dll"));
  ASSERT_TRUE(
      base::WriteFile(patch_dir.Append(child_module::kManifestFilename), "{}"));
}

}  // namespace

class DynamicPatchComponentInstallerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    user_data_dir_ = base::PathService::CheckedGet(chrome::DIR_USER_DATA);

    // The directory into which setup.exe installs patches.
    modules_dir_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("modules"));

    // The directory into which the component installer installed the
    // component, as provided to OnCustomInstall().
    install_dir_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("install"));
    ASSERT_TRUE(base::CreateDirectory(install_dir_));
    ASSERT_TRUE(base::WriteFile(install_dir_.Append(kInnerCrxFileName), "crx"));
  }

  // Returns the directory into which setup.exe installs version
  // `patch_version` of the patch.
  base::FilePath GetPatchDir(std::string_view patch_version) const {
    return modules_dir_.AppendASCII(patch_version);
  }

  std::unique_ptr<ComponentInstallerPolicy> CreatePolicy(
      InstallCallback install_callback = UnexpectedInstall()) const {
    return base::WrapUnique(new DynamicPatchComponentInstallerPolicy(
        user_data_dir_, modules_dir_, std::move(install_callback)));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
  base::ScopedTempDir temp_dir_;
  base::FilePath user_data_dir_;
  base::FilePath modules_dir_;
  base::FilePath install_dir_;
};

TEST_F(DynamicPatchComponentInstallerTest, Properties) {
  std::unique_ptr<ComponentInstallerPolicy> policy = CreatePolicy();

  std::vector<uint8_t> hash;
  policy->GetHash(&hash);
  EXPECT_EQ(update_client::GetCrxIdFromPublicKeyHash(hash), kDynamicPatchCrxId);
  EXPECT_EQ(policy->GetName(), "Chrome Dynamic Patch");
  EXPECT_TRUE(policy->GetInstallerAttributes().empty());
  EXPECT_TRUE(policy->RequiresNetworkEncryption());
  EXPECT_FALSE(policy->SupportsGroupPolicyEnabledComponentUpdates());
  // The CRX cache retains the component so that the update that follows a
  // browser update can be differential.
  EXPECT_TRUE(policy->AllowCachedCopies());
}

TEST_F(DynamicPatchComponentInstallerTest, VerifyInstallation_Installed) {
  ASSERT_NO_FATAL_FAILURE(StagePatch(GetPatchDir(kPatchVersion)));

  EXPECT_TRUE(CreatePolicy()->VerifyInstallation(MakeManifest(kPatchVersion),
                                                 install_dir_));
}

TEST_F(DynamicPatchComponentInstallerTest, VerifyInstallation_NotInstalled) {
  EXPECT_FALSE(CreatePolicy()->VerifyInstallation(MakeManifest(kPatchVersion),
                                                  install_dir_));
}

// setup.exe copies the manifest into place last, so a patch without one has
// not been installed completely.
TEST_F(DynamicPatchComponentInstallerTest, VerifyInstallation_NoManifest) {
  const base::FilePath patch_dir = GetPatchDir(kPatchVersion);
  ASSERT_NO_FATAL_FAILURE(StagePatch(patch_dir));
  ASSERT_TRUE(
      base::DeleteFile(patch_dir.Append(child_module::kManifestFilename)));

  EXPECT_FALSE(CreatePolicy()->VerifyInstallation(MakeManifest(kPatchVersion),
                                                  install_dir_));
}

TEST_F(DynamicPatchComponentInstallerTest, VerifyInstallation_NoRendererDll) {
  const base::FilePath patch_dir = GetPatchDir(kPatchVersion);
  ASSERT_NO_FATAL_FAILURE(StagePatch(patch_dir));
  ASSERT_TRUE(base::DeleteFile(patch_dir.Append(chrome::kRendererDll)));

  EXPECT_FALSE(CreatePolicy()->VerifyInstallation(MakeManifest(kPatchVersion),
                                                  install_dir_));
}

TEST_F(DynamicPatchComponentInstallerTest,
       VerifyInstallation_OtherPatchVersion) {
  ASSERT_NO_FATAL_FAILURE(StagePatch(GetPatchDir("1.2.3.5")));

  EXPECT_FALSE(CreatePolicy()->VerifyInstallation(MakeManifest(kPatchVersion),
                                                  install_dir_));
}

TEST_F(DynamicPatchComponentInstallerTest, VerifyInstallation_BadManifest) {
  ASSERT_NO_FATAL_FAILURE(StagePatch(GetPatchDir(kPatchVersion)));
  std::unique_ptr<ComponentInstallerPolicy> policy = CreatePolicy();

  EXPECT_FALSE(policy->VerifyInstallation(base::DictValue(), install_dir_));
  EXPECT_FALSE(
      policy->VerifyInstallation(MakeManifest("not a version"), install_dir_));
}

// Tests OnCustomInstall() for system-level (true) and per-user (false)
// installs.
class DynamicPatchComponentInstallerOnCustomInstallTest
    : public DynamicPatchComponentInstallerTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  static std::string_view GetInstallLevel(bool system_level) {
    return system_level ? "SystemLevel" : "UserLevel";
  }

  // Expects that a single installation with `result` was recorded for the
  // install level under test, and that none was recorded for the other level.
  void ExpectInstallationRecorded(const base::HistogramTester& histogram_tester,
                                  ProductComponentInstallResult result) const {
    const bool system_level = GetParam();
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"ComponentUpdater.DynamicPatch.InstallationResult.",
                      GetInstallLevel(system_level)}),
        result, 1);
    histogram_tester.ExpectTotalCount(
        base::StrCat({"ComponentUpdater.DynamicPatch.InstallDuration.",
                      GetInstallLevel(system_level)}),
        1);
    histogram_tester.ExpectTotalCount(
        base::StrCat({"ComponentUpdater.DynamicPatch.InstallationResult.",
                      GetInstallLevel(!system_level)}),
        0);
    histogram_tester.ExpectTotalCount(
        base::StrCat({"ComponentUpdater.DynamicPatch.InstallDuration.",
                      GetInstallLevel(!system_level)}),
        0);
  }

  install_static::ScopedInstallDetails install_details_{
      /*system_level=*/GetParam()};
};

TEST_P(DynamicPatchComponentInstallerOnCustomInstallTest, Success) {
  const base::FilePath inner_crx_path = install_dir_.Append(kInnerCrxFileName);
  int install_count = 0;
  std::unique_ptr<ComponentInstallerPolicy> policy = CreatePolicy(
      base::BindLambdaForTesting([&](const base::FilePath& crx_path,
                                     const base::FilePath& user_data_dir) {
        ++install_count;
        EXPECT_EQ(crx_path, inner_crx_path);
        EXPECT_TRUE(base::PathExists(crx_path));
        EXPECT_EQ(user_data_dir, user_data_dir_);
        return ProductComponentInstallResult::kSuccess;
      }));
  base::HistogramTester histogram_tester;

  const Result result =
      policy->OnCustomInstall(MakeManifest(kPatchVersion), install_dir_);

  EXPECT_EQ(install_count, 1);
  EXPECT_EQ(result.result.category, update_client::ErrorCategory::kNone);
  // The inner CRX is deleted once it has been installed.
  EXPECT_FALSE(base::PathExists(inner_crx_path));
  ExpectInstallationRecorded(histogram_tester,
                             ProductComponentInstallResult::kSuccess);
}

// setup.exe reports that the patch already exists if this or a newer version
// was previously installed for the user and User Data directory (e.g., if the
// browser exited before the component updater recorded a previous
// installation). This is not an installation failure.
TEST_P(DynamicPatchComponentInstallerOnCustomInstallTest, AlreadyExists) {
  base::HistogramTester histogram_tester;

  const Result result =
      CreatePolicy(ReturnResult(ProductComponentInstallResult::kAlreadyExists))
          ->OnCustomInstall(MakeManifest(kPatchVersion), install_dir_);

  EXPECT_EQ(result.result.category, update_client::ErrorCategory::kNone);
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kInnerCrxFileName)));
  ExpectInstallationRecorded(histogram_tester,
                             ProductComponentInstallResult::kAlreadyExists);
}

TEST_P(DynamicPatchComponentInstallerOnCustomInstallTest, Failure) {
  base::HistogramTester histogram_tester;

  const Result result =
      CreatePolicy(
          ReturnResult(
              ProductComponentInstallResult::kSignatureVerificationFailed))
          ->OnCustomInstall(MakeManifest(kPatchVersion), install_dir_);

  EXPECT_EQ(result.result.category, update_client::ErrorCategory::kInstaller);
  EXPECT_EQ(result.result.code,
            static_cast<int>(
                ProductComponentInstallResult::kSignatureVerificationFailed));
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kInnerCrxFileName)));
  ExpectInstallationRecorded(
      histogram_tester,
      ProductComponentInstallResult::kSignatureVerificationFailed);
}

INSTANTIATE_TEST_SUITE_P(All,
                         DynamicPatchComponentInstallerOnCustomInstallTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return std::string(info.param ? "SystemLevel"
                                                         : "UserLevel");
                         });

// Tests of MaybeRegisterDynamicPatchComponent(). ComponentInstaller::Register()
// queries the ComponentUpdateService synchronously, so a StrictMock service
// detects an unexpected registration without the need to run any tasks.
class DynamicPatchComponentInstallerMaybeRegisterTest
    : public DynamicPatchComponentInstallerTest {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(DynamicPatchComponentInstallerTest::SetUp());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    // Stand in for the browser, which sets the directory at startup.
    child_module_root_override_.emplace(chrome::DIR_CHILD_MODULE_ROOT);
  }

  // Registers the browser in DIR_EXE as installed at both system and user
  // level.
  static void SetInstalled() {
    const std::wstring setup_path = base::PathService::CheckedGet(base::DIR_EXE)
                                        .AppendASCII(kOtherBaseVersion)
                                        .Append(installer::kInstallerDir)
                                        .Append(installer::kSetupExe)
                                        .value();
    for (const HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
      ASSERT_EQ(
          base::win::RegKey(root, install_static::GetClientsKeyPath().c_str(),
                            KEY_SET_VALUE | KEY_WOW64_32KEY)
              .WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
          ERROR_SUCCESS);
      ASSERT_EQ(
          base::win::RegKey(root,
                            install_static::GetClientStateKeyPath().c_str(),
                            KEY_SET_VALUE | KEY_WOW64_32KEY)
              .WriteValue(installer::kUninstallStringField, setup_path.c_str()),
          ERROR_SUCCESS);
    }
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
  base::ScopedPathOverride exe_dir_override_{base::DIR_EXE};
  base::ScopedPathOverride exe_override_{
      base::FILE_EXE,
      base::PathService::CheckedGet(base::DIR_EXE)
          .Append(installer::kChromeExe),
      /*is_absolute=*/true, /*create=*/false};
  std::optional<base::ScopedPathOverride> child_module_root_override_;
};

TEST_F(DynamicPatchComponentInstallerMaybeRegisterTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(child_module::features::kDynamicPatching);
  ASSERT_NO_FATAL_FAILURE(SetInstalled());
  StrictMock<MockComponentUpdateService> cus;

  MaybeRegisterDynamicPatchComponent(&cus);
}

// setup.exe installs patches into the registered installation of the browser,
// so the component is not registered for a browser that isn't registered.
TEST_F(DynamicPatchComponentInstallerMaybeRegisterTest, NotInstalled) {
  base::test::ScopedFeatureList feature_list(
      child_module::features::kDynamicPatching);
  StrictMock<MockComponentUpdateService> cus;

  MaybeRegisterDynamicPatchComponent(&cus);
}

// The component is not registered if the browser could not determine where it
// looks for patches.
TEST_F(DynamicPatchComponentInstallerMaybeRegisterTest, NoChildModuleRoot) {
  base::test::ScopedFeatureList feature_list(
      child_module::features::kDynamicPatching);
  ASSERT_NO_FATAL_FAILURE(SetInstalled());
  child_module_root_override_.reset();
  StrictMock<MockComponentUpdateService> cus;

  MaybeRegisterDynamicPatchComponent(&cus);
}

TEST_F(DynamicPatchComponentInstallerMaybeRegisterTest, Installed) {
  base::test::ScopedFeatureList feature_list(
      child_module::features::kDynamicPatching);
  ASSERT_NO_FATAL_FAILURE(SetInstalled());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  NiceMock<MockComponentUpdateService> cus;
  ON_CALL(cus, GetRegisteredVersion(_))
      .WillByDefault(Return(base::Version(kNullVersion)));
  ON_CALL(cus, GetMaxPreviousProductVersion(_))
      .WillByDefault(Return(base::Version(kNullVersion)));
  base::RunLoop run_loop;
  EXPECT_CALL(cus, RegisterComponent(Field(&ComponentRegistration::app_id,
                                           std::string(kDynamicPatchCrxId))))
      .WillOnce([&run_loop](const ComponentRegistration&) {
        run_loop.Quit();
        return true;
      });

  MaybeRegisterDynamicPatchComponent(&cus);
  // The installer registers the component after looking for an installed
  // version of it in the background.
  run_loop.Run();
#else
  // Patches are only delivered to Google Chrome.
  StrictMock<MockComponentUpdateService> cus;

  MaybeRegisterDynamicPatchComponent(&cus);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace component_updater
