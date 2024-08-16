// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media_galleries/chromeos/mtp_device_object_enumerator.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct MtpFileEntryData {
  const char* const name;
  int64_t size;
  bool is_directory;
  time_t modification_time;
};

const MtpFileEntryData kTestCases[] = {
  { "Foo", 123, false, 321 },
  { "Bar", 234, true, 432 },
  { "Baaz", 345, false, 543 },
};

void TestEnumeratorIsEmpty(MTPDeviceObjectEnumerator* enumerator) {
  EXPECT_EQ(0, enumerator->Size());
  EXPECT_FALSE(enumerator->IsDirectory());
  EXPECT_TRUE(enumerator->LastModifiedTime().is_null());
}

void TestNextEntryIsEmpty(MTPDeviceObjectEnumerator* enumerator) {
  EXPECT_TRUE(enumerator->Next().empty());
}

typedef testing::Test MTPDeviceObjectEnumeratorTest;

TEST_F(MTPDeviceObjectEnumeratorTest, Empty) {
  std::vector<device::mojom::MtpFileEntryPtr> entries;
  MTPDeviceObjectEnumerator enumerator(std::move(entries));
  TestEnumeratorIsEmpty(&enumerator);
  TestNextEntryIsEmpty(&enumerator);
  TestNextEntryIsEmpty(&enumerator);
  TestEnumeratorIsEmpty(&enumerator);
}

TEST_F(MTPDeviceObjectEnumeratorTest, Traversal) {
  std::vector<device::mojom::MtpFileEntryPtr> entries;
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    auto entry = device::mojom::MtpFileEntry::New();
    entry->file_name = kTestCases[i].name;
    entry->file_size = kTestCases[i].size;
    entry->file_type =
        kTestCases[i].is_directory
            ? device::mojom::MtpFileEntry::FileType::FILE_TYPE_FOLDER
            : device::mojom::MtpFileEntry::FileType::FILE_TYPE_OTHER;
    entry->modification_time = kTestCases[i].modification_time;
    entries.push_back(std::move(entry));
  }
  MTPDeviceObjectEnumerator enumerator(std::move(entries));
  TestEnumeratorIsEmpty(&enumerator);
  TestEnumeratorIsEmpty(&enumerator);
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    EXPECT_EQ(kTestCases[i].name, enumerator.Next().value());
    EXPECT_EQ(kTestCases[i].size, enumerator.Size());
    EXPECT_EQ(kTestCases[i].is_directory, enumerator.IsDirectory());
    EXPECT_EQ(kTestCases[i].modification_time,
              enumerator.LastModifiedTime().ToTimeT());
  }
  TestNextEntryIsEmpty(&enumerator);
  TestNextEntryIsEmpty(&enumerator);
  TestEnumeratorIsEmpty(&enumerator);
  TestNextEntryIsEmpty(&enumerator);
  TestEnumeratorIsEmpty(&enumerator);
}

}  // namespace
