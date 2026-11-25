// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/library_source.h"

#include <memory>
#include <utility>

#include "base/byte_size.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/on_device_translation/component_updater_library_source.h"
#include "chrome/browser/on_device_translation/installer_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace on_device_translation {
namespace {

// An absolute path on every platform.
#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kLibraryPath[] =
    FILE_PATH_LITERAL("C:\\library\\translatekit.dll");
#else
constexpr base::FilePath::CharType kLibraryPath[] =
    FILE_PATH_LITERAL("/library/libtranslatekit.so");
#endif  // BUILDFLAG(IS_WIN)

// A library source that tests control.
class FakeLibrarySource : public TranslateKitLibrarySource {
 public:
  FakeLibrarySource() = default;
  ~FakeLibrarySource() override = default;

  // TranslateKitLibrarySource:
  base::FilePath GetLibraryPath() const override { return library_path_; }
  void Install() override { ++install_count_; }

  // Installs the library at `library_path` and tells the client.
  void FinishInstall(base::FilePath library_path) {
    library_path_ = std::move(library_path);
    client()->OnLibraryChanged();
  }

  // Tells the client that the install failed.
  void FailInstall(InstallError error) {
    client()->OnLibraryInstallFailed(error);
  }

  int install_count() const { return install_count_; }

 private:
  base::FilePath library_path_;
  int install_count_ = 0;
};

class MockObserver : public OnDeviceTranslationInstaller::Observer {
 public:
  MOCK_METHOD(void, OnLanguagePackInstalled, (LanguagePackKey), (override));
  MOCK_METHOD(void,
              OnLanguagePackInstallationChanged,
              (LanguagePackKey),
              (override));
  MOCK_METHOD(void, OnInstallationChanged, (), (override));
  MOCK_METHOD(void, OnLibraryInstallFailed, (InstallError), (override));
};

class TranslateKitLibrarySourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    // Don't depend on the form factor of the device running the test.
    base::android::device_info::set_is_automotive_for_testing(false);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // Creates an installer that gets the library from a `FakeLibrarySource`.
  void CreateInstaller() {
    auto library_source = std::make_unique<FakeLibrarySource>();
    library_source_ = library_source.get();
    installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>(
        std::move(library_source));
  }

  void TearDown() override {
#if BUILDFLAG(IS_ANDROID)
    base::android::device_info::reset_is_automotive_for_testing();
#endif  // BUILDFLAG(IS_ANDROID)
    library_source_ = nullptr;
    installer_.reset();
    TestingBrowserProcess::GetGlobal()->local_state()->ClearPref(
        prefs::kTranslateKitBinaryPath);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<OnDeviceTranslationInstallerImpl> installer_;
  raw_ptr<FakeLibrarySource> library_source_ = nullptr;
};

TEST_F(TranslateKitLibrarySourceTest, InstallerUsesLibraryFromSource) {
  CreateInstaller();
  EXPECT_FALSE(installer_->IsInit());

  library_source_->FinishInstall(base::FilePath(kLibraryPath));
  EXPECT_TRUE(installer_->IsInit());
  EXPECT_EQ(installer_->GetLibraryPath(), base::FilePath(kLibraryPath));
}

TEST_F(TranslateKitLibrarySourceTest, InitWaitsForInstall) {
  CreateInstaller();
  MockObserver observer;
  installer_->AddObserver(&observer);

  base::MockCallback<base::RepeatingClosure> on_ready1;
  base::MockCallback<base::RepeatingClosure> on_ready2;
  installer_->Init(on_ready1.Get());
  installer_->Init(on_ready2.Get());

  // Concurrent `Init()` calls coalesce into a single connectivity check and
  // install request.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return library_source_->install_count() == 1; }));

  EXPECT_CALL(on_ready1, Run());
  EXPECT_CALL(on_ready2, Run());
  EXPECT_CALL(observer, OnInstallationChanged());
  library_source_->FinishInstall(base::FilePath(kLibraryPath));
  EXPECT_TRUE(installer_->IsInit());
  installer_->RemoveObserver(&observer);
}

// Tests that `Init()` asks the source again while an install runs. The source
// ignores the request if its install still runs, and starts a new one if the
// earlier one ended without a report.
TEST_F(TranslateKitLibrarySourceTest, InitAsksSourceWhileInstallRuns) {
  CreateInstaller();
  installer_->Init(base::RepeatingClosure());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return library_source_->install_count() == 1; }));

  installer_->Init(base::RepeatingClosure());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return library_source_->install_count() == 2; }));
}

TEST_F(TranslateKitLibrarySourceTest, InitDropsCallbacksWhenInstallFails) {
  CreateInstaller();
  MockObserver observer;
  installer_->AddObserver(&observer);

  base::MockCallback<base::RepeatingClosure> on_ready1;
  EXPECT_CALL(on_ready1, Run()).Times(0);
  installer_->Init(on_ready1.Get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return library_source_->install_count() == 1; }));
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kFailed));
  library_source_->FailInstall(InstallError::kFailed);
  EXPECT_FALSE(installer_->IsInit());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // The next `Init()` retries the install.
  base::MockCallback<base::RepeatingClosure> on_ready2;
  installer_->Init(on_ready2.Get());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return library_source_->install_count() == 2; }));
  EXPECT_CALL(on_ready2, Run());
  EXPECT_CALL(observer, OnInstallationChanged());
  library_source_->FinishInstall(base::FilePath(kLibraryPath));
  installer_->RemoveObserver(&observer);
}

TEST_F(TranslateKitLibrarySourceTest, InitRunsCallbackWhenInstalled) {
  CreateInstaller();
  library_source_->FinishInstall(base::FilePath(kLibraryPath));

  base::MockCallback<base::RepeatingClosure> on_ready;
  EXPECT_CALL(on_ready, Run());
  installer_->Init(on_ready.Get());
  EXPECT_EQ(library_source_->install_count(), 0);
}

TEST_F(TranslateKitLibrarySourceTest, CreateUsesPathFromCommandLine) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      kTranslateKitBinaryPath, base::FilePath(kLibraryPath));

  std::unique_ptr<TranslateKitLibrarySource> library_source =
      TranslateKitLibrarySource::Create();
  EXPECT_EQ(library_source->GetLibraryPath(), base::FilePath(kLibraryPath));
}

TEST_F(TranslateKitLibrarySourceTest, UnsupportedWithLittleMemory) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kTranslationAPI,
      {{"TranslationAPIMinPhysicalMemoryMB", "2048"}});
  CreateInstaller();
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory(base::MiB(1900));
    EXPECT_FALSE(installer_->IsSupported());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory(base::MiB(3800));
    EXPECT_TRUE(installer_->IsSupported());
  }
}

TEST_F(TranslateKitLibrarySourceTest, SupportedWhenMinPhysicalMemoryDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kTranslationAPI,
      {{"TranslationAPIMinPhysicalMemoryMB", "0"}});
  CreateInstaller();
  base::test::ScopedAmountOfPhysicalMemoryOverride memory(base::MiB(1900));
  EXPECT_TRUE(installer_->IsSupported());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(TranslateKitLibrarySourceTest, UnsupportedOnAutomotive) {
  base::test::ScopedAmountOfPhysicalMemoryOverride memory(base::MiB(4096));
  CreateInstaller();
  EXPECT_TRUE(installer_->IsSupported());

  base::android::device_info::set_is_automotive_for_testing(true);
  EXPECT_FALSE(installer_->IsSupported());

  MockObserver observer;
  installer_->AddObserver(&observer);
  EXPECT_CALL(observer, OnLibraryInstallFailed(InstallError::kFailed));
  installer_->Init(base::RepeatingClosure());
  EXPECT_EQ(library_source_->install_count(), 0);
  installer_->RemoveObserver(&observer);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(TranslateKitLibrarySourceTest, FixedPathSourceWithoutPathIsUnsupported) {
  EXPECT_TRUE(
      FixedPathLibrarySource(base::FilePath(kLibraryPath)).IsSupported());
  EXPECT_FALSE(FixedPathLibrarySource(base::FilePath()).IsSupported());
}

TEST_F(TranslateKitLibrarySourceTest, ComponentUpdaterSourceReadsPref) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>(
      std::make_unique<ComponentUpdaterLibrarySource>(local_state));
  MockObserver observer;
  installer_->AddObserver(&observer);
  EXPECT_FALSE(installer_->IsInit());

  EXPECT_CALL(observer, OnInstallationChanged());
  local_state->SetFilePath(prefs::kTranslateKitBinaryPath,
                           base::FilePath(kLibraryPath));
  EXPECT_TRUE(installer_->IsInit());
  EXPECT_EQ(installer_->GetLibraryPath(), base::FilePath(kLibraryPath));
  installer_->RemoveObserver(&observer);
}

// Tests that a relative path in the pref, which earlier versions stored while
// the component wasn't installed yet, doesn't count as an installed library.
TEST_F(TranslateKitLibrarySourceTest,
       ComponentUpdaterSourceIgnoresRelativePath) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetFilePath(prefs::kTranslateKitBinaryPath,
                           base::FilePath(FILE_PATH_LITERAL(
                               "TranslateKitFiles/libtranslatekit.so")));
  installer_ = std::make_unique<OnDeviceTranslationInstallerImpl>(
      std::make_unique<ComponentUpdaterLibrarySource>(local_state));
  EXPECT_FALSE(installer_->IsInit());
}

// Tests that destroying `PrefService` before `ComponentUpdaterLibrarySource`
// (as `TestingBrowserProcess::~TestingBrowserProcess()` does) leaves no
// dangling pointer.
TEST_F(TranslateKitLibrarySourceTest,
       ComponentUpdaterSourceSurvivesPrefServiceDestruction) {
  auto local_state = std::make_unique<TestingPrefServiceSimple>();
  RegisterLocalStatePrefs(local_state->registry());
  ComponentUpdaterLibrarySource source(local_state.get());
  local_state.reset();
  EXPECT_TRUE(source.GetLibraryPath().empty());
}

}  // namespace
}  // namespace on_device_translation
