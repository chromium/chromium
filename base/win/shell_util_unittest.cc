// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/shell_util.h"

#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/win/elevation_util.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

TEST(ShellUtilTest, RunShellExecuteViaExplorer) {
  base::test::TaskEnvironment task_environment;
  ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().Append(L"success.txt");

  // Watch the directory since the file does not exist yet.
  base::FilePathWatcher watcher;
  base::RunLoop run_loop;
  ASSERT_TRUE(watcher.Watch(
      temp_dir.GetPath(), base::FilePathWatcher::Type::kNonRecursive,
      base::BindRepeating(
          [](base::RepeatingClosure quit_closure,
             const base::FilePath& temp_file, const base::FilePath& path,
             bool error) {
            if (error) {
              return;
            }
            std::string content;
            // Only quit the loop if the file exists and contains the expected
            // content. This handles the case where the file is created empty
            // first.
            if (base::ReadFileToString(temp_file, &content) &&
                content.find("success") != std::string::npos) {
              quit_closure.Run();
            }
          },
          run_loop.QuitClosure(), temp_file)));

  base::FilePath sys_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &sys_dir));
  base::FilePath cmd_exe = sys_dir.Append(L"cmd.exe");

  std::wstring parameters =
      base::StrCat({L"/c echo success > \"", temp_file.value(), L"\""});

  ShellExecuteOptions options{.start_hidden = true};

  ASSERT_HRESULT_SUCCEEDED(
      RunShellExecuteViaExplorer(cmd_exe.value(), parameters, options));

  // Wait for the file to be created and written. base::RunLoop::Run() will
  // return immediately if the watcher already called Quit().
  run_loop.Run();
}

}  // namespace base::win
