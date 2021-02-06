// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "apps/saved_files_service.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#define TRACE_CALL(expression) \
  do {                         \
    SCOPED_TRACE(#expression); \
    expression;                \
  } while (0)

using apps::SavedFilesService;
using extensions::SavedFileEntry;

namespace {

std::string GenerateId(int i) {
  return base::NumberToString(i) + ":filename.ext";
}

}  // namespace

class SavedFilesServiceUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    extension_ = env_.MakeExtension(*base::test::ParseJsonDeprecated(
        "{"
        "  \"app\": {"
        "    \"background\": {"
        "      \"scripts\": [\"background.js\"]"
        "    }"
        "  },"
        "  \"permissions\": ["
        "    {\"fileSystem\": [\"retainEntries\"]}"
        "  ]"
        "}"));
    service_ = SavedFilesService::Get(env_.profile());
    path_ = base::FilePath(FILE_PATH_LITERAL("filename.ext"));
  }

  void TearDown() override {
    SavedFilesService::ClearMaxSequenceNumberForTest();
    SavedFilesService::ClearLruSizeForTest();
    testing::Test::TearDown();
  }

  // Check that a registered file entry has the correct value.
  void CheckEntrySequenceNumber(int id, int sequence_number) {
    std::string id_string = GenerateId(id);
    SCOPED_TRACE(id_string);
    EXPECT_TRUE(service_->IsRegistered(extension_->id(), id_string));
    const SavedFileEntry* entry =
        service_->GetFileEntry(extension_->id(), id_string);
    ASSERT_TRUE(entry);
    EXPECT_EQ(id_string, entry->id);
    EXPECT_EQ(path_, entry->path);
    EXPECT_TRUE(entry->is_directory);
    EXPECT_EQ(sequence_number, entry->sequence_number);
  }

  // Check that a range of registered file entries have the correct values.
  void CheckRangeEnqueuedInOrder(int start, int end) {
    SavedFileEntry entry;
    for (int i = start; i < end; i++) {
      CheckEntrySequenceNumber(i, i + 1);
    }
  }

  extensions::TestExtensionEnvironment env_;
  const extensions::Extension* extension_;
  SavedFilesService* service_;
  base::FilePath path_;
};

TEST_F(SavedFilesServiceUnitTest, RetainTwoFilesTest) {
  service_->RegisterFileEntry(extension_->id(), GenerateId(1), path_, true);
  service_->RegisterFileEntry(extension_->id(), GenerateId(2), path_, true);
  service_->RegisterFileEntry(extension_->id(), GenerateId(3), path_, true);

  // Test that no entry has a sequence number.
  TRACE_CALL(CheckEntrySequenceNumber(1, 0));
  TRACE_CALL(CheckEntrySequenceNumber(2, 0));
  TRACE_CALL(CheckEntrySequenceNumber(3, 0));

  // Test that only entry #1 has a sequence number.
  service_->EnqueueFileEntry(extension_->id(), GenerateId(1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  TRACE_CALL(CheckEntrySequenceNumber(2, 0));

  // Test that entry #1 has not changed sequence number because it is the most
  // recently enqueued entry.
  service_->EnqueueFileEntry(extension_->id(), GenerateId(1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  TRACE_CALL(CheckEntrySequenceNumber(2, 0));

  // Test that entry #1 is unchanged and entry #2 has been assigned the next
  // sequence number.
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));

  // Test that both entries #1 and #2 are unchanged because #2 is the most
  // recently enqueued entry.
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));

  // Test that entry #1 has been assigned the next sequence number.
  service_->EnqueueFileEntry(extension_->id(), GenerateId(1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 3));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));
  TRACE_CALL(CheckEntrySequenceNumber(3, 0));

  EXPECT_FALSE(service_->IsRegistered(extension_->id(), "another id"));
  SavedFileEntry entry;
  EXPECT_FALSE(service_->GetFileEntry(extension_->id(), "another id"));

  // ClearQueueIfNoRetainPermission should be a no-op because the app has the
  // fileSystem.retainEntries permission.
  service_->ClearQueueIfNoRetainPermission(extension_);
  TRACE_CALL(CheckEntrySequenceNumber(1, 3));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));
  TRACE_CALL(CheckEntrySequenceNumber(3, 0));

  // Test that after a clear, retained file entries are unchanged, but file
  // entries that have been registered but not retained are no longer
  // registered.
  service_->Clear(extension_->id());
  TRACE_CALL(CheckEntrySequenceNumber(1, 3));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));
  EXPECT_FALSE(service_->IsRegistered(extension_->id(), GenerateId(3)));
}

TEST_F(SavedFilesServiceUnitTest, NoRetainEntriesPermissionTest) {
  extension_ = env_.MakeExtension(*base::test::ParseJsonDeprecated(
      "{\"app\": {\"background\": {\"scripts\": [\"background.js\"]}},"
      "\"permissions\": [\"fileSystem\"]}"));
  service_->RegisterFileEntry(extension_->id(), GenerateId(1), path_, true);
  TRACE_CALL(CheckEntrySequenceNumber(1, 0));
  SavedFileEntry entry;
  service_->EnqueueFileEntry(extension_->id(), GenerateId(1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  EXPECT_FALSE(service_->IsRegistered(extension_->id(), "another id"));
  EXPECT_FALSE(service_->GetFileEntry(extension_->id(), "another id"));

  // ClearQueueIfNoRetainPermission should clear the queue, since the app does
  // not have the "retainEntries" permission.
  service_->ClearQueueIfNoRetainPermission(extension_);
  std::vector<SavedFileEntry> entries =
      service_->GetAllFileEntries(extension_->id());
  EXPECT_TRUE(entries.empty());
}

TEST_F(SavedFilesServiceUnitTest, EvictionTest) {
  SavedFilesService::SetLruSizeForTest(10);
  for (int i = 0; i < 10; i++) {
    service_->RegisterFileEntry(extension_->id(), GenerateId(i), path_, true);
    service_->EnqueueFileEntry(extension_->id(), GenerateId(i));
  }
  service_->RegisterFileEntry(extension_->id(), GenerateId(10), path_, true);

  // Expect that entries 0 to 9 are in the queue, but 10 is not.
  TRACE_CALL(CheckRangeEnqueuedInOrder(0, 10));
  TRACE_CALL(CheckEntrySequenceNumber(10, 0));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(10));

  // Expect that entries 1 to 10 are in the queue, but entry 0 is not.
  TRACE_CALL(CheckEntrySequenceNumber(0, 0));
  TRACE_CALL(CheckRangeEnqueuedInOrder(1, 11));

  // Check that retained entries are unchanged after a clear.
  service_->Clear(extension_->id());
  SavedFileEntry entry;
  EXPECT_FALSE(service_->GetFileEntry(extension_->id(), GenerateId(0)));
  TRACE_CALL(CheckRangeEnqueuedInOrder(1, 11));

  // Expect that entry 2 is now at the back of the queue, and no further entries
  // have been evicted.
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));
  TRACE_CALL(CheckEntrySequenceNumber(2, 12));
  TRACE_CALL(CheckRangeEnqueuedInOrder(1, 1));
  TRACE_CALL(CheckRangeEnqueuedInOrder(3, 11));

  // Check that retained entries are unchanged after a clear.
  service_->Clear(extension_->id());
  TRACE_CALL(CheckEntrySequenceNumber(2, 12));
  TRACE_CALL(CheckRangeEnqueuedInOrder(1, 1));
  TRACE_CALL(CheckRangeEnqueuedInOrder(3, 11));
}

TEST_F(SavedFilesServiceUnitTest, SequenceNumberCompactionTest) {
  SavedFilesService::SetMaxSequenceNumberForTest(8);
  SavedFilesService::SetLruSizeForTest(8);
  for (int i = 0; i < 4; i++) {
    service_->RegisterFileEntry(extension_->id(), GenerateId(i), path_, true);
    service_->EnqueueFileEntry(extension_->id(), GenerateId(i));
  }
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(3));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));

  // The sequence numbers should be sparse, as they have not gone over the
  // limit.
  TRACE_CALL(CheckEntrySequenceNumber(0, 1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 2));
  TRACE_CALL(CheckEntrySequenceNumber(2, 7));
  TRACE_CALL(CheckEntrySequenceNumber(3, 6));
  service_->Clear(extension_->id());
  TRACE_CALL(CheckEntrySequenceNumber(0, 1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 2));
  TRACE_CALL(CheckEntrySequenceNumber(2, 7));
  TRACE_CALL(CheckEntrySequenceNumber(3, 6));

  // This should push the sequence number to the limit of 8, and trigger a
  // sequence number compaction. Expect that the sequence numbers are
  // contiguous from 1 to 4.
  service_->EnqueueFileEntry(extension_->id(), GenerateId(3));
  TRACE_CALL(CheckRangeEnqueuedInOrder(0, 4));
  service_->Clear(extension_->id());
  TRACE_CALL(CheckRangeEnqueuedInOrder(0, 4));
}
