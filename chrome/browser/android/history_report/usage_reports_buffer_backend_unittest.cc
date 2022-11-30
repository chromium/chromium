// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/usage_reports_buffer_backend.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/android/history_report/usage_report_util.h"
#include "chrome/browser/android/proto/delta_file.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
void VerifyUsageReport(history_report::UsageReport& actual,
                       const std::string& expected_id,
                       int64_t expected_timestamp_ms,
                       bool expected_typed_visit) {
  EXPECT_EQ(expected_id, actual.id());
  EXPECT_EQ(expected_timestamp_ms, actual.timestamp_ms());
  EXPECT_EQ(expected_typed_visit, actual.typed_visit());
}
}  //  namespace

namespace history_report {

class UsageReportsBufferBackendTest : public testing::Test {
 public:
  UsageReportsBufferBackendTest() {}

  UsageReportsBufferBackendTest(const UsageReportsBufferBackendTest&) = delete;
  UsageReportsBufferBackendTest& operator=(
      const UsageReportsBufferBackendTest&) = delete;

  ~UsageReportsBufferBackendTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    buffer_ = std::make_unique<UsageReportsBufferBackend>(temp_dir_.GetPath());
    EXPECT_TRUE(buffer_->Init());
  }

  std::unique_ptr<UsageReportsBufferBackend> buffer_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(UsageReportsBufferBackendTest, AddTypedVisit) {
  buffer_->AddVisit("id", 7, true);

  std::unique_ptr<std::vector<UsageReport>> result =
      buffer_->GetUsageReportsBatch(1);

  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  VerifyUsageReport((*result)[0], "id", 7, true);
}

TEST_F(UsageReportsBufferBackendTest, AddNotTypedVisit) {
  buffer_->AddVisit("id", 7, false);

  std::unique_ptr<std::vector<UsageReport>> result =
      buffer_->GetUsageReportsBatch(1);

  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  VerifyUsageReport((*result)[0], "id", 7, false);
}

TEST_F(UsageReportsBufferBackendTest, GetUsageReportsBatchNotEnoughReports) {
  buffer_->AddVisit("id", 7, true);
  buffer_->AddVisit("id2", 10, false);
  buffer_->AddVisit("id3", 12, false);
  buffer_->AddVisit("id4", 5, true);

  std::unique_ptr<std::vector<UsageReport>> result =
      buffer_->GetUsageReportsBatch(5);

  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(4u, result->size());
  std::set<std::string> ids;
  for (std::vector<UsageReport>::iterator it = result->begin();
       it != result->end();
       ++it) {
    ids.insert(it->id());
    if (it->id() == "id") {
      VerifyUsageReport(*it, "id", 7, true);
      continue;
    }
    if (it->id() == "id2") {
      VerifyUsageReport(*it, "id2", 10, false);
      continue;
    }
    if (it->id() == "id3") {
      VerifyUsageReport(*it, "id3", 12, false);
      continue;
    }
    if (it->id() == "id4") {
      VerifyUsageReport(*it, "id4", 5, true);
      continue;
    }
    FAIL();
  }
  EXPECT_EQ(4u, ids.size());
}

TEST_F(UsageReportsBufferBackendTest, GetUsageReportsBatchTooManyReports) {
  buffer_->AddVisit("id", 7, true);
  buffer_->AddVisit("id2", 10, false);
  buffer_->AddVisit("id3", 12, false);
  buffer_->AddVisit("id4", 5, true);

  std::unique_ptr<std::vector<UsageReport>> result =
      buffer_->GetUsageReportsBatch(3);

  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(3u, result->size());
  std::set<std::string> ids;
  for (std::vector<UsageReport>::iterator it = result->begin();
       it != result->end();
       ++it) {
    ids.insert(it->id());
    if (it->id() == "id") {
      VerifyUsageReport(*it, "id", 7, true);
      continue;
    }
    if (it->id() == "id2") {
      VerifyUsageReport(*it, "id2", 10, false);
      continue;
    }
    if (it->id() == "id3") {
      VerifyUsageReport(*it, "id3", 12, false);
      continue;
    }
    if (it->id() == "id4") {
      VerifyUsageReport(*it, "id4", 5, true);
      continue;
    }
    FAIL();
  }
  EXPECT_EQ(3u, ids.size());
}

TEST_F(UsageReportsBufferBackendTest, Remove) {
  buffer_->AddVisit("id", 7, true);

  std::unique_ptr<std::vector<UsageReport>> result =
      buffer_->GetUsageReportsBatch(1);

  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  VerifyUsageReport((*result)[0], "id", 7, true);

  // Query for a second time to make sure that previous query didn't remove
  // anything from the buffer.
  result = buffer_->GetUsageReportsBatch(1);

  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  VerifyUsageReport((*result)[0], "id", 7, true);

  std::vector<std::string> to_remove;

  for (std::vector<UsageReport>::const_iterator it = result->begin();
       it != result->end();
       ++it) {
    to_remove.push_back(usage_report_util::ReportToKey(*it));
  }

  buffer_->Remove(to_remove);

  result = buffer_->GetUsageReportsBatch(1);

  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(0u, result->size());
}

TEST_F(UsageReportsBufferBackendTest, Persistence) {
  buffer_->AddVisit("id", 7, true);

  std::unique_ptr<std::vector<UsageReport>> result =
      buffer_->GetUsageReportsBatch(2);

  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  VerifyUsageReport((*result)[0], "id", 7, true);

  buffer_.reset(NULL);
  buffer_ = std::make_unique<UsageReportsBufferBackend>(temp_dir_.GetPath());
  EXPECT_TRUE(buffer_->Init());

  result = buffer_->GetUsageReportsBatch(2);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  VerifyUsageReport((*result)[0], "id", 7, true);
}

}  // namespace history_report
