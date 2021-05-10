// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources_integrity.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

class CheckResourceIntegrityTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CheckResourceIntegrityTest, Match) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));

  std::array<uint8_t, crypto::kSHA256Length> expected = {
      0x1b, 0x3a, 0x5c, 0x9f, 0x92, 0x74, 0x48, 0xcc, 0x89, 0x1a, 0xe8,
      0x3e, 0xcb, 0xfa, 0xc6, 0x6e, 0xb8, 0x73, 0x03, 0xf2, 0xb2, 0x25,
      0xee, 0xf3, 0xba, 0x7f, 0xb6, 0x94, 0x85, 0x61, 0xe2, 0xe8};

  base::RunLoop loop;
  CheckResourceIntegrity(test_data_path.AppendASCII("circle.svg"), expected,
                         base::BindLambdaForTesting([&](bool matches) {
                           EXPECT_TRUE(matches);
                           loop.Quit();
                         }));
  loop.Run();
}

TEST_F(CheckResourceIntegrityTest, Mismatch) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));

  std::vector<uint8_t> unexpected(crypto::kSHA256Length, 'a');
  base::RunLoop loop;
  CheckResourceIntegrity(test_data_path.AppendASCII("circle.svg"),
                         base::make_span<32>(unexpected),
                         base::BindLambdaForTesting([&](bool matches) {
                           EXPECT_FALSE(matches);
                           loop.Quit();
                         }));
  loop.Run();
}

TEST_F(CheckResourceIntegrityTest, NonExistentFile) {
  std::vector<uint8_t> unexpected(crypto::kSHA256Length, 'a');
  base::RunLoop loop;
  CheckResourceIntegrity(
      base::FilePath(FILE_PATH_LITERAL("this file does not exist.moo")),
      base::make_span<crypto::kSHA256Length>(unexpected),
      base::BindLambdaForTesting([&](bool matches) {
        EXPECT_FALSE(matches);
        loop.Quit();
      }));
  loop.Quit();
}

#if BUILDFLAG(ENABLE_PAK_FILE_INTEGRITY_CHECKS)
TEST_F(CheckResourceIntegrityTest, ChromePaks) {
  base::HistogramTester tester;
  CheckPakFileIntegrity();
  task_environment_.RunUntilIdle();
  tester.ExpectBucketCount("SafeBrowsing.PakIntegrity.Resources", 1, 1);
  tester.ExpectBucketCount("SafeBrowsing.PakIntegrity.Chrome100", 1, 1);
  tester.ExpectBucketCount("SafeBrowsing.PakIntegrity.Chrome200", 1, 1);
}
#endif  // BUILDFLAG(ENABLE_PAK_FILE_INTEGRITY_CHECKS)
