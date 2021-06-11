// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/persistent_histogram_storage.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Name of the allocator for storing histograms.
constexpr char kTestHistogramAllocatorName[] = "TestMetrics";

}  // namespace

class PersistentHistogramStorageTest : public testing::Test {
 protected:
  PersistentHistogramStorageTest() = default;
  ~PersistentHistogramStorageTest() override = default;

  // Creates a unique temporary directory, and sets the test storage directory.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_storage_dir_ =
        temp_dir_path().AppendASCII(kTestHistogramAllocatorName);
  }

  // Gets the path to the temporary directory.
  const FilePath& temp_dir_path() { return temp_dir_.GetPath(); }

  const FilePath& test_storage_dir() { return test_storage_dir_; }

 private:
  // A temporary directory where all file IO operations take place.
  ScopedTempDir temp_dir_;

  // The directory into which metrics files are written.
  FilePath test_storage_dir_;

  DISALLOW_COPY_AND_ASSIGN(PersistentHistogramStorageTest);
};

#if !defined(OS_NACL)
TEST_F(PersistentHistogramStorageTest, HistogramWriteTest) {
  auto persistent_histogram_storage =
      std::make_unique<PersistentHistogramStorage>(
          kTestHistogramAllocatorName,
          PersistentHistogramStorage::StorageDirManagement::kCreate);

  persistent_histogram_storage->set_storage_base_dir(temp_dir_path());

  // Log some random data.
  UMA_HISTOGRAM_BOOLEAN("Some.Test.Metric", true);

  // Deleting the object causes the data to be written to the disk.
  persistent_histogram_storage.reset();

  // The storage directory and the histogram file are created during the
  // destruction of the PersistentHistogramStorage instance.
  EXPECT_TRUE(DirectoryExists(test_storage_dir()));
  EXPECT_FALSE(IsDirectoryEmpty(test_storage_dir()));

  // Clean up for subsequent tests.
  GlobalHistogramAllocator::ReleaseForTesting();
}
#endif  // !defined(OS_NACL)

}  // namespace base
