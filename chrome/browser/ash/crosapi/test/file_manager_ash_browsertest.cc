// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_manager_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "content/public/test/browser_test.h"

namespace crosapi {
namespace {

using FileManagerCrosapiTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(FileManagerCrosapiTest, ShowItemInFolder) {
  // TODO(crbug.com/1351650): Add success cases in file manager crosapi test
  base::FilePath bad_path("/does/not/exist");
  base::RunLoop run_loop1;

  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->ShowItemInFolder(
      bad_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedPathNotFound);
            quit_closure.Run();
          },
          run_loop1.QuitClosure()));
  run_loop1.Run();

  base::RunLoop run_loop2;
  base::FilePath malformed_path("!@#$%");

  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->ShowItemInFolder(
      malformed_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedPathNotFound);
            quit_closure.Run();
          },
          run_loop2.QuitClosure()));
  run_loop2.Run();
}

IN_PROC_BROWSER_TEST_F(FileManagerCrosapiTest, OpenFolder) {
  // TODO(crbug.com/1351650): Add success cases in file manager crosapi test
  base::FilePath bad_path("/does/not/exist");
  base::RunLoop run_loop;

  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFolder(
      bad_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedPathNotFound);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FileManagerCrosapiTest, OpenFile) {
  // TODO(crbug.com/1351650): Add success cases in file manager crosapi test
  base::FilePath bad_path("/does/not/exist");
  base::RunLoop run_loop;

  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFile(
      bad_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedPathNotFound);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace
}  // namespace crosapi
