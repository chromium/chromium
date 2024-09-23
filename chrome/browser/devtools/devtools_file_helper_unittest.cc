// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_helper.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/selected_file_info.h"

class DevToolsFileHelperTest : public content::RenderViewHostTestHarness,
                               public DevToolsFileHelper::Delegate {
 public:
  void FileSystemAdded(
      const std::string& error,
      const DevToolsFileHelper::FileSystem* file_system) override {}
  void FileSystemRemoved(const std::string& file_system_path) override {}
  void FilePathsChanged(
      const std::vector<std::string>& changed_paths,
      const std::vector<std::string>& added_paths,
      const std::vector<std::string>& removed_paths) override {}

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Setup bits and bobs so we can instantiate a DevToolsFileHelper. It needs
    // a WebContents, Profile and permission to show the "Save as" dialog.
    auto* profile = Profile::FromBrowserContext(browser_context());
    testing_local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());
    testing_local_state_->Get()->SetBoolean(prefs::kAllowFileSelectionDialogs,
                                            true);
    DownloadCoreServiceFactory::GetForBrowserContext(profile)
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile));
    DownloadPrefs::FromBrowserContext(browser_context())
        ->SetDownloadPath(temp_dir_.GetPath());
    file_helper_ =
        std::make_unique<DevToolsFileHelper>(web_contents(), profile, this);
  }

  void TearDown() override {
    // Release the helper first, otherwise the profile will be a dangling ptr.
    file_helper_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<DevToolsFileHelper> file_helper_;
  std::unique_ptr<ScopedTestingLocalState> testing_local_state_;
};

TEST_F(DevToolsFileHelperTest, SaveToFileBase64) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("test.wasm");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{path}));

  base::RunLoop run_loop;
  file_helper_->Save(
      "https://example.com/test.wasm", "AGFzbQEAAAA=", /* save_as */ true,
      /* is_base64 */ true,
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  const std::vector<uint8_t> kTestData = {0, 'a', 's', 'm', 1, 0, 0, 0};
  ASSERT_EQ(base::ReadFileToBytes(path), kTestData);
}

TEST_F(DevToolsFileHelperTest, SaveToFileInvalidBase64) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("test.wasm");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{path}));

  file_helper_->Save(
      "https://example.com/test.wasm", "~~~~", /* save_as */ true,
      /* is_base64 */ true, base::DoNothing(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(base::PathExists(path));
}

TEST_F(DevToolsFileHelperTest, SaveToFileText) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("test.txt");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{path}));

  base::RunLoop run_loop;
  file_helper_->Save(
      "https://example.com/test.txt", "some text", /* save_as */ true,
      /* is_base64 */ false,
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  ASSERT_EQ(content, "some text");
}
