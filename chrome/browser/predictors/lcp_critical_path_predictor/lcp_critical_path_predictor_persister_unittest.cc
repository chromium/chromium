// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_persister.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that writing, saving to disk, reinitializing, and reading yields the
// written value.
TEST(LCPCriticalPathPredictorPersister, PutReinitializeAndGet) {
  base::test::TaskEnvironment env;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("to_be_deleted"));

  GURL main_page("https://example.com/");

  // Initialize database and write a value.
  {
    base::test::TestFuture<std::unique_ptr<LCPCriticalPathPredictorPersister>>
        persister;
    LCPCriticalPathPredictorPersister::CreateForFilePath(
        base::SingleThreadTaskRunner::GetCurrentDefault(), temp_path,
        /*flush_delay_for_writes=*/base::TimeDelta(), persister.GetCallback());
    env.RunUntilIdle();  // Allow initialization to complete.
    ASSERT_TRUE(persister.Wait());

    LCPElement lcp_element;
    lcp_element.set_lcp_element_url("https://example.com/lcp.png");
    persister.Get()->SetLCPElement(main_page, lcp_element);

    env.RunUntilIdle();  // Allow the write to persist.
  }

  // Re-initialize database and read the written value.
  base::test::TestFuture<std::unique_ptr<LCPCriticalPathPredictorPersister>>
      persister;
  LCPCriticalPathPredictorPersister::CreateForFilePath(
      base::SingleThreadTaskRunner::GetCurrentDefault(), temp_path,
      /*flush_delay_for_writes=*/base::TimeDelta(), persister.GetCallback());
  env.RunUntilIdle();  // Allow initialization to complete.
  ASSERT_TRUE(persister.Wait());

  absl::optional<LCPElement> got = persister.Get()->GetLCPElement(main_page);
  ASSERT_TRUE(got);
  EXPECT_EQ(got->lcp_element_url(), "https://example.com/lcp.png");

  persister.Take().reset();
  // Wait until the persister's database finishes closing its
  // database asynchronously, so as not to leak after the test concludes.
  env.RunUntilIdle();
}
