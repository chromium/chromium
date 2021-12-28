// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class FirstPartySetsUtilTest : public ::testing::Test {
 public:
  FirstPartySetsUtilTest() {
    CHECK(scoped_dir_.CreateUniqueTempDir());
    CHECK(PathExists(scoped_dir_.GetPath()));

    persisted_sets_path_ = scoped_dir_.GetPath().Append(
        FILE_PATH_LITERAL("persisted_first_party_sets.json"));
  }

 protected:
  base::ScopedTempDir scoped_dir_;
  base::FilePath persisted_sets_path_;
  base::test::TaskEnvironment env_;
};

TEST_F(FirstPartySetsUtilTest, SendAndUpdatePersistedSets_FileNotExist) {
  SEQUENCE_CHECKER(sequence_checker);
  const std::string expected_updated_sets = "updated first party sets";

  FirstPartySetsUtil::GetInstance()->SendAndUpdatePersistedSets(
      scoped_dir_.GetPath(),
      /*send_sets=*/
      base::BindLambdaForTesting(
          [&](base::OnceCallback<void(const std::string&)> callback,
              const std::string& got) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
            EXPECT_EQ(got, "");
            std::move(callback).Run(expected_updated_sets);
          }));

  env_.RunUntilIdle();

  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_EQ(got, expected_updated_sets);
}

TEST_F(FirstPartySetsUtilTest, SendAndUpdatePersistedSets) {
  SEQUENCE_CHECKER(sequence_checker);
  const std::string expected_read_sets = "persisted first party sets";
  const std::string expected_updated_sets = "updated first party sets";

  ASSERT_TRUE(base::WriteFile(persisted_sets_path_, expected_read_sets));

  FirstPartySetsUtil::GetInstance()->SendAndUpdatePersistedSets(
      scoped_dir_.GetPath(),
      /*send_sets=*/
      base::BindLambdaForTesting(
          [&](base::OnceCallback<void(const std::string&)> callback,
              const std::string& got) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
            EXPECT_EQ(got, expected_read_sets);
            std::move(callback).Run(expected_updated_sets);
          }));

  env_.RunUntilIdle();

  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_EQ(got, expected_updated_sets);
}
