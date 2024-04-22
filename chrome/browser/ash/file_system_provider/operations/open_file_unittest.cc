// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/open_file.h"

#include <memory>
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
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("/directory/blueberries.txt");

// Callback invocation logger. Acts as a fileapi end-point.
class CallbackLogger {
 public:
  class Event {
   public:
    Event(int file_handle,
          base::File::Error result,
          std::unique_ptr<EntryMetadata> metadata)
        : file_handle_(file_handle),
          result_(result),
          metadata_(std::move(metadata)) {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    virtual ~Event() = default;

    int file_handle() { return file_handle_; }
    base::File::Error result() { return result_; }
    EntryMetadata* metadata() { return metadata_.get(); }

   private:
    int file_handle_;
    base::File::Error result_;
    std::unique_ptr<EntryMetadata> metadata_;
  };

  CallbackLogger() = default;

  CallbackLogger(const CallbackLogger&) = delete;
  CallbackLogger& operator=(const CallbackLogger&) = delete;

  virtual ~CallbackLogger() = default;

  void OnOpenFile(int file_handle,
                  base::File::Error result,
                  std::unique_ptr<EntryMetadata> metadata) {
    events_.push_back(
        std::make_unique<Event>(file_handle, result, std::move(metadata)));
  }

  std::vector<std::unique_ptr<Event>>& events() { return events_; }

 private:
  std::vector<std::unique_ptr<Event>> events_;
};

}  // namespace

class FileSystemProviderOperationsOpenFileTest : public testing::Test {
 protected:
  FileSystemProviderOperationsOpenFileTest() = default;
  ~FileSystemProviderOperationsOpenFileTest() override = default;

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, MountOptions(kFileSystemId, /*display_name=*/""),
        base::FilePath(), /*configurable=*/false, /*watchable=*/true,
        extensions::SOURCE_FILE, IconSet());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsOpenFileTest, Execute) {
  using extensions::api::file_system_provider::OpenFileRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                     OPEN_FILE_MODE_READ,
                     base::BindOnce(&CallbackLogger::OnOpenFile,
                                    base::Unretained(&callback_logger)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(
      extensions::api::file_system_provider::OnOpenFileRequested::kEventName,
      event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  auto options =
      OpenFileRequestedOptions::FromValue(options_as_value->GetDict());
  ASSERT_TRUE(options);
  EXPECT_EQ(kFileSystemId, options->file_system_id);
  EXPECT_EQ(kRequestId, options->request_id);
  EXPECT_EQ(kFilePath, options->file_path);
  EXPECT_EQ(extensions::api::file_system_provider::OpenFileMode::kRead,
            options->mode);
}

TEST_F(FileSystemProviderOperationsOpenFileTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/false);
  CallbackLogger callback_logger;

  OpenFile open_file(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                     OPEN_FILE_MODE_READ,
                     base::BindOnce(&CallbackLogger::OnOpenFile,
                                    base::Unretained(&callback_logger)));

  EXPECT_FALSE(open_file.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsOpenFileTest, Execute_ReadOnly) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  const ProvidedFileSystemInfo read_only_file_system_info(
      kExtensionId, MountOptions(kFileSystemId, /*display_name=*/""),
      /*mount_path=*/base::FilePath(), /*configurable=*/false,
      /*watchable=*/true, extensions::SOURCE_FILE, IconSet());

  // Opening for read on a read-only file system is allowed.
  {
    OpenFile open_file(&dispatcher, read_only_file_system_info,
                       base::FilePath(kFilePath), OPEN_FILE_MODE_READ,
                       base::BindOnce(&CallbackLogger::OnOpenFile,
                                      base::Unretained(&callback_logger)));

    EXPECT_TRUE(open_file.Execute(kRequestId));
  }

  // Opening for write on a read-only file system is forbidden and must fail.
  {
    OpenFile open_file(&dispatcher, read_only_file_system_info,
                       base::FilePath(kFilePath), OPEN_FILE_MODE_WRITE,
                       base::BindOnce(&CallbackLogger::OnOpenFile,
                                      base::Unretained(&callback_logger)));

    EXPECT_FALSE(open_file.Execute(kRequestId));
  }
}

TEST_F(FileSystemProviderOperationsOpenFileTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  OpenFile open_file(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                     OPEN_FILE_MODE_READ,
                     base::BindOnce(&CallbackLogger::OnOpenFile,
                                    base::Unretained(&callback_logger)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  open_file.OnSuccess(kRequestId, RequestValue(), false /* has_more */);
  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(base::File::FILE_OK, event->result());
  EXPECT_LT(0, event->file_handle());
}

TEST_F(FileSystemProviderOperationsOpenFileTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  OpenFile open_file(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                     OPEN_FILE_MODE_READ,
                     base::BindOnce(&CallbackLogger::OnOpenFile,
                                    base::Unretained(&callback_logger)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  open_file.OnError(kRequestId, RequestValue(),
                    base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
  ASSERT_EQ(0, event->file_handle());
}

}  // namespace ash::file_system_provider::operations
