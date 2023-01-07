// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/file_manager.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using FileManagerLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(FileManagerLacrosBrowserTest, Basics) {
  crosapi::mojom::FileManagerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::FileManager>()
          .get());
  crosapi::mojom::OpenResult result;

  // The file manager requires a large amount of setup to get it to run in
  // tests. See FileManagerBrowserTestBase. For example, by default it won't
  // show files at arbitrary paths, like the sorts of temporary files you can
  // safely create in a test. Since we don't have a test mojo API to put the
  // file manager into a suitable state, exercise our API via error cases.
  base::FilePath bad_path("/does/not/exist");
  waiter.ShowItemInFolder(bad_path, &result);
  EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound, result);

  waiter.OpenFolder(bad_path, &result);
  EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound, result);

  waiter.OpenFile(bad_path, &result);
  EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound, result);

  base::FilePath malformed_path("!@#$%");
  waiter.ShowItemInFolder(malformed_path, &result);
  EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound, result);
}
