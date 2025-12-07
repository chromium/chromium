// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_select_file_dialog.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

using ::testing::Return;

class DevToolsSelectFileDialogTest : public content::RenderViewHostTestHarness {
 protected:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    content::RenderViewHostTestHarness::TearDown();
  }
};

TEST_F(DevToolsSelectFileDialogTest, SelectFileCanceledCallback) {
  base::RunLoop run_loop;
  content::SelectFileDialogParams params;
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::CancellingSelectFileDialogFactory>(&params));
  base::MockCallback<DevToolsSelectFileDialog::CanceledCallback> cb;
  EXPECT_CALL(cb, Run()).Times(1).WillOnce([&] { run_loop.Quit(); });

  DevToolsSelectFileDialog::SelectFile(
      web_contents(), ui::SelectFileDialog::SELECT_FOLDER, base::DoNothing(),
      cb.Get(), base::FilePath());
  run_loop.Run();

  EXPECT_EQ(params.type, ui::SelectFileDialog::SELECT_FOLDER);
}

TEST_F(DevToolsSelectFileDialogTest, SelectFileSelectedCallback) {
  base::RunLoop run_loop;
  content::SelectFileDialogParams params;
  const base::FilePath test_path = base::FilePath::FromASCII("test.txt");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{test_path}, &params));

  base::test::TestFuture<const ui::SelectedFileInfo&> future;
  DevToolsSelectFileDialog::SelectFile(
      web_contents(), ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      future.GetCallback(), base::DoNothing(), base::FilePath());
  EXPECT_TRUE(future.Wait());
  ui::SelectedFileInfo result = future.Get();
  EXPECT_EQ(result.file_path, test_path);

  EXPECT_EQ(params.type, ui::SelectFileDialog::SELECT_SAVEAS_FILE);
  EXPECT_TRUE(params.default_path.empty());
}

TEST_F(DevToolsSelectFileDialogTest,
       SelectFileSelectedCallbackWithDefaultPath) {
  base::RunLoop run_loop;
  content::SelectFileDialogParams params;
  const base::FilePath test_path = base::FilePath::FromASCII("test.html");
  const base::FilePath default_path = base::FilePath::FromASCII("default.html");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{test_path}, &params));

  base::test::TestFuture<const ui::SelectedFileInfo&> future;
  DevToolsSelectFileDialog::SelectFile(
      web_contents(), ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      future.GetCallback(), base::DoNothing(), default_path);
  EXPECT_TRUE(future.Wait());
  ui::SelectedFileInfo result = future.Get();
  EXPECT_EQ(result.file_path, test_path);

  EXPECT_EQ(params.type, ui::SelectFileDialog::SELECT_SAVEAS_FILE);
  EXPECT_EQ(params.default_path, default_path);
}
