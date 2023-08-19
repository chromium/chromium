// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/file_manager.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using FileManagerLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(FileManagerLacrosBrowserTest, Basics) {
  auto& file_manager =
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::FileManager>();
  base::test::TestFuture<crosapi::mojom::OpenResult> result_future;

  // The file manager requires a large amount of setup to get it to run in
  // tests. See FileManagerBrowserTestBase. For example, by default it won't
  // show files at arbitrary paths, like the sorts of temporary files you can
  // safely create in a test. Since we don't have a test mojo API to put the
  // file manager into a suitable state, exercise our API via error cases.
  base::FilePath bad_path("/does/not/exist");
  file_manager->ShowItemInFolder(bad_path, result_future.GetCallback());
  EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound,
            result_future.Take());

  file_manager->OpenFolder(bad_path, result_future.GetCallback());
  EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound,
            result_future.Take());

  file_manager->OpenFile(bad_path, result_future.GetCallback());
  EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound,
            result_future.Take());

  base::FilePath malformed_path("!@#$%");
  file_manager->ShowItemInFolder(malformed_path, result_future.GetCallback());
  EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound,
            result_future.Take());
}
