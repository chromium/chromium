// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_zipfile_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#endif

namespace extensions {

namespace {

struct MockExtensionRegistryObserver : public ExtensionRegistryObserver {
  void WaitForInstall(bool expect_error) {
    extensions::LoadErrorReporter* error_reporter =
        extensions::LoadErrorReporter::GetInstance();
    error_reporter->ClearErrors();
    while (true) {
      base::RunLoop run_loop;
      // We do not get a notification if installation fails. Make sure to wake
      // up and check for errors to get an error better than the test
      // timing-out.
      // TODO(jcivelli): make LoadErrorReporter::Observer report installation
      // failures for packaged extensions so we don't have to poll.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
      quit_closure = run_loop.QuitClosure();
      run_loop.Run();
      const std::vector<std::u16string>* errors = error_reporter->GetErrors();
      if (!errors->empty()) {
        if (!expect_error) {
          FAIL() << "Error(s) happened when unzipping extension: "
                 << (*errors)[0];
        }
        break;
      }
      if (!last_extension_installed.empty()) {
        // Extension install succeeded.
        EXPECT_FALSE(expect_error);
        break;
      }
    }
  }

  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override {
    last_extension_installed = extension->id();
    last_extension_installed_path = extension->path();
    std::move(quit_closure).Run();
  }

  std::string last_extension_installed;
  base::FilePath last_extension_installed_path;
  base::OnceClosure quit_closure;
};

struct UnzipFileFilterTestCase {
  const base::FilePath::CharType* input;
  const bool should_unzip;
};

}  // namespace

// Assists with testing the non-installation location behavior of the installer.
class ZipFileInstallerTest : public ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    extensions::LoadErrorReporter::Init(/*enable_noisy_errors=*/false);
    in_process_utility_thread_helper_ =
        std::make_unique<content::InProcessUtilityThreadHelper>();
    unzip::SetUnzipperLaunchOverrideForTesting(
        base::BindRepeating(&unzip::LaunchInProcessUnzipper));
    registry()->AddObserver(&observer_);
  }

  void TearDown() override {
    registry()->RemoveObserver(&observer_);
    ExtensionServiceTestBase::TearDown();
    // Need to destruct ZipFileInstaller before the message loop since
    // it posts a task to it.
    zipfile_installer_.reset();
    unzip::SetUnzipperLaunchOverrideForTesting(base::NullCallback());
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_refptr<ZipFileInstaller> zipfile_installer_;

  std::unique_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;
  MockExtensionRegistryObserver observer_;

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

// Assists with testing the zip file filtering behavior of ZipFileInstaller.
class ZipFileInstallerFilterTest : public ZipFileInstallerTest {
 protected:
  void RunZipFileFilterTest(
      const std::vector<UnzipFileFilterTestCase>& cases,
      base::RepeatingCallback<bool(const base::FilePath&)>& filter) {
    for (size_t i = 0; i < cases.size(); ++i) {
      base::FilePath input(cases[i].input);
      bool observed = filter.Run(input);
      EXPECT_EQ(cases[i].should_unzip, observed)
          << "i: " << i << ", input: " << input.value();
    }
  }
};

TEST_F(ZipFileInstallerFilterTest, NonTheme_FileExtractionFilter) {
  const std::vector<UnzipFileFilterTestCase> cases = {
      {FILE_PATH_LITERAL("foo"), true},
      {FILE_PATH_LITERAL("foo.nexe"), true},
      {FILE_PATH_LITERAL("foo.dll"), true},
      {FILE_PATH_LITERAL("foo.jpg.exe"), false},
      {FILE_PATH_LITERAL("foo.exe"), false},
      {FILE_PATH_LITERAL("foo.EXE"), false},
      {FILE_PATH_LITERAL("file_without_extension"), true},
  };
  base::RepeatingCallback<bool(const base::FilePath&)> filter =
      base::BindRepeating(&ZipFileInstaller::ShouldExtractFile, false);
  RunZipFileFilterTest(cases, filter);
}

TEST_F(ZipFileInstallerFilterTest, Theme_FileExtractionFilter) {
  const std::vector<UnzipFileFilterTestCase> cases = {
      {FILE_PATH_LITERAL("image.jpg"), true},
      {FILE_PATH_LITERAL("IMAGE.JPEG"), true},
      {FILE_PATH_LITERAL("test/image.bmp"), true},
      {FILE_PATH_LITERAL("test/IMAGE.gif"), true},
      {FILE_PATH_LITERAL("test/image.WEBP"), true},
      {FILE_PATH_LITERAL("test/dir/file.image.png"), true},
      {FILE_PATH_LITERAL("manifest.json"), true},
      {FILE_PATH_LITERAL("other.html"), false},
      {FILE_PATH_LITERAL("file_without_extension"), true},
  };
  base::RepeatingCallback<bool(const base::FilePath&)> filter =
      base::BindRepeating(&ZipFileInstaller::ShouldExtractFile, true);
  RunZipFileFilterTest(cases, filter);
}

TEST_F(ZipFileInstallerFilterTest, ManifestExtractionFilter) {
  const std::vector<UnzipFileFilterTestCase> cases = {
      {FILE_PATH_LITERAL("manifest.json"), true},
      {FILE_PATH_LITERAL("MANIFEST.JSON"), true},
      {FILE_PATH_LITERAL("test/manifest.json"), false},
      {FILE_PATH_LITERAL("manifest.json/test"), false},
      {FILE_PATH_LITERAL("other.file"), false},
  };
  base::RepeatingCallback<bool(const base::FilePath&)> filter =
      base::BindRepeating(&ZipFileInstaller::IsManifestFile);
  RunZipFileFilterTest(cases, filter);
}

class ZipFileInstallerLocationTest : public ZipFileInstallerTest,
                                     public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    ZipFileInstallerTest::SetUp();
    expected_extension_install_directory_ =
        service()->unpacked_install_directory();
  }

  // Install the .zip in the test directory with `zip_name` and `expect_error`
  // if it should fail.
  void RunInstaller(const std::string& zip_name,
                    bool expect_error,
                    base::FilePath unzip_dir_root = base::FilePath());

 protected:
  base::FilePath expected_extension_install_directory_;
};

void ZipFileInstallerLocationTest::RunInstaller(const std::string& zip_name,
                                                bool expect_error,
                                                base::FilePath unzip_dir_root) {
  base::FilePath original_zip_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &original_zip_path));
  original_zip_path = original_zip_path.AppendASCII("extensions")
                          .AppendASCII("zipfile_installer")
                          .AppendASCII(zip_name);
  ASSERT_TRUE(base::PathExists(original_zip_path)) << original_zip_path.value();
  zipfile_installer_ = ZipFileInstaller::Create(
      GetExtensionFileTaskRunner(),
      MakeRegisterInExtensionServiceCallback(service()));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ZipFileInstaller::InstallZipFileToUnpackedExtensionsDir,
                     zipfile_installer_, original_zip_path,
                     unzip_dir_root.empty()
                         ? service()->unpacked_install_directory()
                         : unzip_dir_root));
  observer_.WaitForInstall(expect_error);
  task_environment()->RunUntilIdle();
}

// Tests that a normal .zip is installed into the expected install path.
TEST_F(ZipFileInstallerLocationTest, GoodZip) {
  RunInstaller(/*zip_name=*/"good.zip",
               /*expect_error=*/false);

  // Expect extension install directory to be immediate subdir of expected
  // temp install directory. E.g. /a/b/c/d == /a/b/c + /d.
  //
  // Make sure we're comparing absolute paths to avoid failures like
  // https://crbug.com/1453669 on macOS 14.
  base::FilePath absolute_last_extension_installed_path =
      base::MakeAbsoluteFilePath(observer_.last_extension_installed_path);
  base::FilePath absolute_expected_extension_install_directory =
      base::MakeAbsoluteFilePath(expected_extension_install_directory_.Append(
          observer_.last_extension_installed_path.BaseName()));
  EXPECT_EQ(absolute_last_extension_installed_path,
            absolute_expected_extension_install_directory);
}

TEST_F(ZipFileInstallerLocationTest, BadZip) {
  // Manifestless archive.
  RunInstaller(/*zip_name=*/"bad.zip",
               /*expect_error=*/true);
}

// Tests installing the same .zip twice results in two separate install
// directories.
TEST_F(ZipFileInstallerLocationTest, MultipleSameZipInstallSeparately) {
  RunInstaller(/*zip_name=*/"good.zip",
               /*expect_error=*/false);

  base::FilePath dir_temp;
  base::PathService::Get(base::DIR_TEMP, &dir_temp);
  base::FilePath first_install_path = observer_.last_extension_installed_path;
  // Expect extension install directory to be immediate subdir of expected
  // unpacked install directory. E.g. /a/b/c/d == /a/b/c + /d.
  //
  // Make sure we're comparing absolute paths to avoid failures like
  // https://crbug.com/1453669 on macOS 14.
  base::FilePath absolute_last_extension_installed_path =
      base::MakeAbsoluteFilePath(observer_.last_extension_installed_path);
  base::FilePath absolute_expected_extension_install_directory =
      base::MakeAbsoluteFilePath(expected_extension_install_directory_.Append(
          observer_.last_extension_installed_path.BaseName()));
  EXPECT_EQ(absolute_last_extension_installed_path,
            absolute_expected_extension_install_directory);

  RunInstaller(/*zip_name=*/"good.zip",
               /*expect_error=*/false);

  base::FilePath second_install_path = observer_.last_extension_installed_path;
  // Expect extension install directory to be immediate subdir of expected
  // unpacked install directory. E.g. /a/b/c/d == /a/b/c + /d.
  absolute_last_extension_installed_path =
      base::MakeAbsoluteFilePath(observer_.last_extension_installed_path);
  absolute_expected_extension_install_directory =
      base::MakeAbsoluteFilePath(expected_extension_install_directory_.Append(
          observer_.last_extension_installed_path.BaseName()));
  EXPECT_EQ(absolute_last_extension_installed_path,
            absolute_expected_extension_install_directory);

  // Confirm that the two extensions are installed in two separate
  // directories.
  EXPECT_NE(first_install_path, second_install_path);
}

// Tests that we error when we cannot create the parent directory of where to
// install the .zips to.
TEST_F(ZipFileInstallerLocationTest, CannotCreateContainingDirectoryZip) {
  // TODO(crbug.com/40875193): Have this expect a specific error rather than
  // just an error since other things can cause an error.
  RunInstaller(
      /*zip_name=*/"good.zip", /*expect_error=*/true, /*unzip_dir_root=*/
#if !BUILDFLAG(IS_WIN)
      base::FilePath(
          FILE_PATH_LITERAL("/NonExistentDirectory/UnpackedExtensions"))
#else
      // Windows will create unexpected paths so we use explicitly disallowed
      // characters in the Windows filesystem to ensure creating this directory
      // fails.
      base::FilePath(
          FILE_PATH_LITERAL("|<IllegalWinDirName>|/UnpackedExtensions"))
#endif  // !BUILDFLAG(IS_WIN)
  );
}

// Tests that a .zip with a public key installs with the expected extension ID
// and to the correct path.
TEST_F(ZipFileInstallerLocationTest, ZipWithPublicKey) {
  RunInstaller(/*zip_name=*/"public_key.zip",
               /*expect_error=*/false);
  const char kIdForPublicKey[] = "ikppjpenhoddphklkpdfdfdabbakkpal";
  EXPECT_EQ(observer_.last_extension_installed, kIdForPublicKey);

  // Make sure we compare absolute paths to avoid failures like
  // https://crbug.com/1453669 on macOS 14.
  base::FilePath absolute_last_extension_installed_path =
      base::MakeAbsoluteFilePath(observer_.last_extension_installed_path);
  base::FilePath absolute_expected_extension_install_directory =
      base::MakeAbsoluteFilePath(expected_extension_install_directory_.Append(
          observer_.last_extension_installed_path.BaseName()));
  EXPECT_EQ(absolute_last_extension_installed_path,
            absolute_expected_extension_install_directory);
}

}  // namespace extensions
