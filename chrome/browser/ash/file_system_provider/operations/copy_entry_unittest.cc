// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/copy_entry.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/operations/test_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/file_system/async_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider::operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const base::FilePath::CharType kSourcePath[] =
    FILE_PATH_LITERAL("/bunny/and/bear/happy");
const base::FilePath::CharType kTargetPath[] =
    FILE_PATH_LITERAL("/kitty/and/puppy/happy");

}  // namespace

class FileSystemProviderOperationsCopyEntryTest : public testing::Test {
 protected:
  FileSystemProviderOperationsCopyEntryTest() = default;
  ~FileSystemProviderOperationsCopyEntryTest() override = default;

  void SetUp() override {
    MountOptions mount_options(kFileSystemId, /*display_name=*/"");
    mount_options.writable = true;
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, mount_options, base::FilePath(), /*configurable=*/false,
        /*watchable=*/true, extensions::SOURCE_FILE, IconSet());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsCopyEntryTest, Execute) {
  using extensions::api::file_system_provider::CopyEntryRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  util::StatusCallbackLog callback_log;

  CopyEntry copy_entry(&dispatcher, file_system_info_,
                       base::FilePath(kSourcePath), base::FilePath(kTargetPath),
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(copy_entry.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(
      extensions::api::file_system_provider::OnCopyEntryRequested::kEventName,
      event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  auto options =
      CopyEntryRequestedOptions::FromValue(options_as_value->GetDict());
  ASSERT_TRUE(options);
  EXPECT_EQ(kFileSystemId, options->file_system_id);
  EXPECT_EQ(kRequestId, options->request_id);
  EXPECT_EQ(kSourcePath, options->source_path);
  EXPECT_EQ(kTargetPath, options->target_path);
}

TEST_F(FileSystemProviderOperationsCopyEntryTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/false);
  util::StatusCallbackLog callback_log;

  CopyEntry copy_entry(&dispatcher, file_system_info_,
                       base::FilePath(kSourcePath), base::FilePath(kTargetPath),
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_FALSE(copy_entry.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsCopyEntryTest, Execute_ReadOnly) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  util::StatusCallbackLog callback_log;

  const ProvidedFileSystemInfo read_only_file_system_info(
      kExtensionId, MountOptions(kFileSystemId, /*display_name=*/""),
      /*mount_path=*/base::FilePath(), /*configurable=*/false,
      /*watchable=*/true, extensions::SOURCE_FILE, IconSet());

  CopyEntry copy_entry(&dispatcher, read_only_file_system_info,
                       base::FilePath(kSourcePath), base::FilePath(kTargetPath),
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_FALSE(copy_entry.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsCopyEntryTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  util::StatusCallbackLog callback_log;

  CopyEntry copy_entry(&dispatcher, file_system_info_,
                       base::FilePath(kSourcePath), base::FilePath(kTargetPath),
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(copy_entry.Execute(kRequestId));

  copy_entry.OnSuccess(kRequestId, RequestValue(), /*has_more=*/false);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_OK, callback_log[0]);
}

TEST_F(FileSystemProviderOperationsCopyEntryTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  util::StatusCallbackLog callback_log;

  CopyEntry copy_entry(&dispatcher, file_system_info_,
                       base::FilePath(kSourcePath), base::FilePath(kTargetPath),
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(copy_entry.Execute(kRequestId));

  copy_entry.OnError(kRequestId, RequestValue(),
                     base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, callback_log[0]);
}

}  // namespace ash::file_system_provider::operations
