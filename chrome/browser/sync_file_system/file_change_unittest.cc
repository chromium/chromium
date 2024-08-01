// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/sync_file_system/file_change.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {

namespace {

FileChange AddOrUpdateFile() {
  return FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE);
}

FileChange DeleteFile() {
  return FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_FILE);
}

FileChange AddDirectory() {
  return FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                    SYNC_FILE_TYPE_DIRECTORY);
}

FileChange DeleteDirectory() {
  return FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_DIRECTORY);
}

template <size_t INPUT_SIZE>
void CreateList(FileChangeList* list, const FileChange (&inputs)[INPUT_SIZE]) {
  list->clear();
  for (size_t i = 0; i < INPUT_SIZE; ++i)
    list->Update(inputs[i]);
}

template <size_t EXPECTED_SIZE>
void VerifyList(const FileChangeList& list,
                const FileChange (&expected)[EXPECTED_SIZE]) {
  SCOPED_TRACE(testing::Message() << "actual:" << list.DebugString());
  ASSERT_EQ(EXPECTED_SIZE, list.size());
  for (size_t i = 0; i < list.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i << ": "
                 << " expected:" << expected[i].DebugString()
                 << " actual:" << list.list().at(i).DebugString());
    EXPECT_EQ(expected[i], list.list().at(i));
  }
}

}  // namespace

TEST(FileChangeListTest, UpdateSimple) {
  FileChangeList list;
  const FileChange kInput1[] = { AddOrUpdateFile() };
  const FileChange kExpected1[] = { AddOrUpdateFile() };
  CreateList(&list, kInput1);
  VerifyList(list, kExpected1);

  // AddOrUpdate + Delete -> Delete.
  const FileChange kInput2[] = { AddOrUpdateFile(), DeleteFile() };
  const FileChange kExpected2[] = { DeleteFile() };
  CreateList(&list, kInput2);
  VerifyList(list, kExpected2);

  // Add + Delete -> empty (directory).
  const FileChange kInput3[] = { AddDirectory(), DeleteDirectory() };
  CreateList(&list, kInput3);
  ASSERT_TRUE(list.empty());

  // Delete + AddOrUpdate -> AddOrUpdate.
  const FileChange kInput4[] = { DeleteFile(), AddOrUpdateFile() };
  const FileChange kExpected4[] = { AddOrUpdateFile() };
  CreateList(&list, kInput4);
  VerifyList(list, kExpected4);

  // Delete + Add -> Add (directory).
  const FileChange kInput5[] = { DeleteDirectory(), AddDirectory() };
  const FileChange kExpected5[] = { AddDirectory() };
  CreateList(&list, kInput5);
  VerifyList(list, kExpected5);
}

TEST(FileChangeListTest, UpdateCombined) {
  FileChangeList list;

  // Longer ones.
  const FileChange kInput1[] = {
    AddOrUpdateFile(),
    AddOrUpdateFile(),
    AddOrUpdateFile(),
    AddOrUpdateFile(),
    DeleteFile(),
    AddDirectory(),
  };
  const FileChange kExpected1[] = { DeleteFile(), AddDirectory() };
  CreateList(&list, kInput1);
  VerifyList(list, kExpected1);

  const FileChange kInput2[] = {
    AddOrUpdateFile(),
    DeleteFile(),
    AddOrUpdateFile(),
    AddOrUpdateFile(),
    AddOrUpdateFile(),
  };
  const FileChange kExpected2[] = { AddOrUpdateFile() };
  CreateList(&list, kInput2);
  VerifyList(list, kExpected2);

  const FileChange kInput3[] = {
    AddDirectory(),
    DeleteDirectory(),
    AddOrUpdateFile(),
    AddOrUpdateFile(),
    AddOrUpdateFile(),
  };
  const FileChange kExpected3[] = { AddOrUpdateFile() };
  CreateList(&list, kInput3);
  VerifyList(list, kExpected3);

  const FileChange kInput4[] = {
    AddDirectory(),
    DeleteDirectory(),
    AddOrUpdateFile(),
    DeleteFile(),
    AddOrUpdateFile(),
    DeleteFile(),
  };
  const FileChange kExpected4[] = { DeleteFile() };
  CreateList(&list, kInput4);
  VerifyList(list, kExpected4);
}

}  // namespace sync_file_system
