// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/file_upload_impl.h"

#include <string>

#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job_test_util.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

class FileUploadImplTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  FileUploadJob::TestEnvironment manager_test_env_;
};

TEST_F(FileUploadImplTest, Dummy) {
  EXPECT_FALSE(false);
}
}  // namespace
}  // namespace reporting
