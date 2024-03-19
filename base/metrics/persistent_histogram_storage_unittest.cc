// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/persistent_histogram_storage.h"

#include <memory>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_functions.h"
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
 public:
  PersistentHistogramStorageTest(const PersistentHistogramStorageTest&) =
      delete;
  PersistentHistogramStorageTest& operator=(
      const PersistentHistogramStorageTest&) = delete;

 protected:
  PersistentHistogramStorageTest() = default;
  ~PersistentHistogramStorageTest() override = default;

  // Creates a unique temporary directory, and sets the test storage directory.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_storage_dir_ =
        temp_dir_path().AppendASCII(kTestHistogramAllocatorName);
  }

  void TearDown() override {
    // Clean up for subsequent tests.
    GlobalHistogramAllocator::ReleaseForTesting();
  }

  // Gets the path to the temporary directory.
  const FilePath& temp_dir_path() { return temp_dir_.GetPath(); }

  const FilePath& test_storage_dir() { return test_storage_dir_; }

 private:
  // A temporary directory where all file IO operations take place.
  ScopedTempDir temp_dir_;

  // The directory into which metrics files are written.
  FilePath test_storage_dir_;
};

#if !BUILDFLAG(IS_NACL)
TEST_F(PersistentHistogramStorageTest, HistogramWriteTest) {
  auto persistent_histogram_storage =
      std::make_unique<PersistentHistogramStorage>(
          kTestHistogramAllocatorName,
          PersistentHistogramStorage::StorageDirManagement::kCreate);

  persistent_histogram_storage->set_storage_base_dir(temp_dir_path());

  // Log some random data.
  UmaHistogramBoolean("Some.Test.Metric", true);

  // Deleting the object causes the data to be written to the disk.
  persistent_histogram_storage.reset();

  // The storage directory and the histogram file are created during the
  // destruction of the PersistentHistogramStorage instance.
  EXPECT_TRUE(DirectoryExists(test_storage_dir()));
  EXPECT_FALSE(IsDirectoryEmpty(test_storage_dir()));
}

TEST_F(PersistentHistogramStorageTest, TimeCreationTest) {
  // Tests that we can create several PersistentHistogramStorage instances in
  // close time proximity and correctly end with several different files.
  constexpr int kNumStorageInstances = 3;
  for (int i = 0; i < kNumStorageInstances; ++i) {
    auto persistent_histogram_storage =
        std::make_unique<PersistentHistogramStorage>(
            kTestHistogramAllocatorName,
            PersistentHistogramStorage::StorageDirManagement::kCreate);

    persistent_histogram_storage->set_storage_base_dir(temp_dir_path());

    // Log some random data.
    UmaHistogramBoolean("Some.Test.Metric", true);

    // Deleting the object causes the data to be written to the disk.
    persistent_histogram_storage.reset();

    // We need the global allocator to allow us to create a new instance.
    GlobalHistogramAllocator::ReleaseForTesting();
  }

  // The storage directory and the histogram file are created during the
  // destruction of the PersistentHistogramStorage instance.
  EXPECT_TRUE(DirectoryExists(test_storage_dir()));
  EXPECT_FALSE(IsDirectoryEmpty(test_storage_dir()));

  // We should have |kNumStorageInstances| histogram files in the directory.
  FileEnumerator enumerator(
      test_storage_dir(), /*recursive=*/false, FileEnumerator::FILES,
      FilePath(FILE_PATH_LITERAL("*"))
          .AddExtension(PersistentMemoryAllocator::kFileExtension)
          .value());
  int num_files = 0;
  for (auto file = enumerator.Next(); !file.empty(); file = enumerator.Next()) {
    ++num_files;
  }
  EXPECT_EQ(num_files, kNumStorageInstances);
}
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace base
