// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/delta_file_backend_leveldb.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/android/history_report/delta_file_commons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_report {

class DeltaFileBackendTest : public testing::Test {
 public:
  DeltaFileBackendTest() {}

  DeltaFileBackendTest(const DeltaFileBackendTest&) = delete;
  DeltaFileBackendTest& operator=(const DeltaFileBackendTest&) = delete;

  ~DeltaFileBackendTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_ = std::make_unique<DeltaFileBackend>(temp_dir_.GetPath());
  }

  std::unique_ptr<DeltaFileBackend> backend_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(DeltaFileBackendTest, AddPage) {
  GURL test_url("test.org");
  backend_->PageAdded(test_url);
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  DeltaFileEntryWithData data = (*result)[0];
  EXPECT_EQ(1, data.SeqNo());
  EXPECT_EQ(data.Type(), "add");
  EXPECT_EQ(data.Url(), test_url.spec());
}

TEST_F(DeltaFileBackendTest, DelPage) {
  GURL test_url("test.org");
  backend_->PageDeleted(test_url);
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  DeltaFileEntryWithData data = (*result)[0];
  EXPECT_EQ(1, data.SeqNo());
  EXPECT_EQ(data.Type(), "del");
  EXPECT_EQ(data.Url(), test_url.spec());
}

TEST_F(DeltaFileBackendTest, Recreate) {
  GURL test_url("test.org");
  // Adding 5 deletion entries for test.org
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);

  std::vector<std::string> urls;
  urls.push_back("test.org");
  urls.push_back("test2.org");
  EXPECT_TRUE(backend_->Recreate(urls));
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(2u, result->size());
  DeltaFileEntryWithData data = (*result)[0];
  EXPECT_EQ(1, data.SeqNo());
  EXPECT_EQ(data.Type(), "add");
  EXPECT_EQ(data.Url(), "test.org");
  DeltaFileEntryWithData data2 = (*result)[1];
  EXPECT_EQ(2, data2.SeqNo());
  EXPECT_EQ(data2.Type(), "add");
  EXPECT_EQ(data2.Url(), "test2.org");
}

TEST_F(DeltaFileBackendTest, Clear) {
  GURL test_url("test.org");
  // Adding 5 deletion entries for test.org
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->Clear();

  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(0u, result->size());
}

TEST_F(DeltaFileBackendTest, QueryStart) {
  GURL test_url("test.org");
  // Adding 5 deletion entries for test.org
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);

  // Skip first entry (start with sequence number == 2).
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(1, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(4u, result->size());
  DeltaFileEntryWithData data = (*result)[0];
  // Check that first result is the second entry we added.
  EXPECT_EQ(2, data.SeqNo());
  EXPECT_EQ(data.Type(), "del");
  EXPECT_EQ(data.Url(), test_url.spec());
}

TEST_F(DeltaFileBackendTest, QueryLimit) {
  GURL test_url("test.org");
  // Adding 5 deletion entries for test.org
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);

  // Query for up to 3 results.
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 3);
  EXPECT_TRUE(result.get() != NULL);
  // Check that we got exactly 3 results
  EXPECT_EQ(3u, result->size());
  // Verify that first result is an entry we added first.
  DeltaFileEntryWithData data = (*result)[0];
  EXPECT_EQ(1, data.SeqNo());
  EXPECT_EQ(data.Type(), "del");
  EXPECT_EQ(data.Url(), test_url.spec());
}

TEST_F(DeltaFileBackendTest, Trim) {
  GURL test_url("test.org");
  // Adding 5 deletion entries for test.org
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  // Trim all entries with sequence number <= 3.
  int64_t max_seq_no = backend_->Trim(3);
  EXPECT_EQ(5, max_seq_no);
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(2u, result->size());
  DeltaFileEntryWithData data = (*result)[0];
  // First entry in delta file should now have sequence number == 4 because
  // entries with smaller sequence numbers were deleted.
  EXPECT_EQ(4, data.SeqNo());
  EXPECT_EQ(data.Type(), "del");
  EXPECT_EQ(data.Url(), test_url.spec());
}

TEST_F(DeltaFileBackendTest, TrimLowerBoundEqualToMaxSeqNo) {
  GURL test_url("test.org");
  // Adding 5 deletion entries for test.org
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  // Trim all entries with sequence number <= 5 but leave at least one entry
  // in delta file.
  int64_t max_seq_no = backend_->Trim(5);
  EXPECT_EQ(5, max_seq_no);
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  DeltaFileEntryWithData data = (*result)[0];
  // All entries but last were removed.
  EXPECT_EQ(5, data.SeqNo());
  EXPECT_EQ(data.Type(), "del");
  EXPECT_EQ(data.Url(), test_url.spec());
}

TEST_F(DeltaFileBackendTest, TrimLowerBoundGreaterThanMaxSeqNo) {
  GURL test_url("test.org");
  // Adding 5 deletion entries for test.org
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  backend_->PageDeleted(test_url);
  // Trim all entries with sequence number <= 6 but leave at least one entry
  // in delta file.
  int64_t max_seq_no = backend_->Trim(6);
  EXPECT_EQ(5, max_seq_no);
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  DeltaFileEntryWithData data = (*result)[0];
  // All entries but last were removed.
  EXPECT_EQ(5, data.SeqNo());
  EXPECT_EQ(data.Type(), "del");
  EXPECT_EQ(data.Url(), test_url.spec());
}

TEST_F(DeltaFileBackendTest, TrimDeltaFileWithSingleEntry) {
  GURL test_url("test.org");
  backend_->PageDeleted(test_url);
  // Trim all entries with sequence number <= 1 but leave at least one entry
  // in delta file. Should not remove any entries since there's only one
  // in delta file.
  int64_t max_seq_no = backend_->Trim(1);
  EXPECT_EQ(1, max_seq_no);
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(0, 10);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(1u, result->size());
  DeltaFileEntryWithData data = (*result)[0];
  // No entries removed because there was only one.
  EXPECT_EQ(1, data.SeqNo());
  EXPECT_EQ(data.Type(), "del");
  EXPECT_EQ(data.Url(), test_url.spec());
}

TEST_F(DeltaFileBackendTest, LevelDbComparator) {
  GURL test_url("test.org");
  // Adding 50 deletion entries for test.org
  for (int i = 0; i < 50; i++) {
    backend_->PageDeleted(test_url);
  }

  // Skip first entry (start with sequence number == 2).
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result =
      backend_->Query(1, 100);
  EXPECT_TRUE(result.get() != NULL);
  EXPECT_EQ(49u, result->size());
  for (int i = 0; i < 49; i++) {
    DeltaFileEntryWithData data = (*result)[i];
    // +2 because we skipped first entry and
    // sequence number starts with 1 not with 0.
    EXPECT_EQ(i + 2, data.SeqNo());
    EXPECT_EQ(data.Type(), "del");
    EXPECT_EQ(data.Url(), test_url.spec());
  }
}

} // namespace history_report
