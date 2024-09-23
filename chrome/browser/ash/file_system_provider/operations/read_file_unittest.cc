// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/read_file.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/operations/test_util.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/async_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider::operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const int kFileHandle = 3;
const int kOffset = 10;
const int kLength = 5;

// Callback invocation logger. Acts as a fileapi end-point.
class CallbackLogger {
 public:
  class Event {
   public:
    Event(int chunk_length, bool has_more, base::File::Error result)
        : chunk_length_(chunk_length), has_more_(has_more), result_(result) {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    virtual ~Event() = default;

    int chunk_length() const { return chunk_length_; }
    bool has_more() const { return has_more_; }
    base::File::Error result() const { return result_; }

   private:
    int chunk_length_;
    bool has_more_;
    base::File::Error result_;
  };

  CallbackLogger() = default;

  CallbackLogger(const CallbackLogger&) = delete;
  CallbackLogger& operator=(const CallbackLogger&) = delete;

  virtual ~CallbackLogger() = default;

  void OnReadFile(int chunk_length, bool has_more, base::File::Error result) {
    events_.push_back(std::make_unique<Event>(chunk_length, has_more, result));
  }

  std::vector<std::unique_ptr<Event>>& events() { return events_; }

 private:
  std::vector<std::unique_ptr<Event>> events_;
};

}  // namespace

class FileSystemProviderOperationsReadFileTest : public testing::Test {
 protected:
  FileSystemProviderOperationsReadFileTest() = default;
  ~FileSystemProviderOperationsReadFileTest() override = default;

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, MountOptions(kFileSystemId, /*display_name=*/""),
        base::FilePath(), /*configurable=*/false, /*watchable=*/true,
        extensions::SOURCE_FILE, IconSet());
    io_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kOffset + kLength);
  }

  ProvidedFileSystemInfo file_system_info_;
  scoped_refptr<net::IOBuffer> io_buffer_;
};

TEST_F(FileSystemProviderOperationsReadFileTest, Execute) {
  using extensions::api::file_system_provider::ReadFileRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  ReadFile read_file(&dispatcher, file_system_info_, kFileHandle,
                     io_buffer_.get(), kOffset, kLength,
                     base::BindRepeating(&CallbackLogger::OnReadFile,
                                         base::Unretained(&callback_logger)));

  EXPECT_TRUE(read_file.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(
      extensions::api::file_system_provider::OnReadFileRequested::kEventName,
      event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  auto options =
      ReadFileRequestedOptions::FromValue(options_as_value->GetDict());
  ASSERT_TRUE(options);
  EXPECT_EQ(kFileSystemId, options->file_system_id);
  EXPECT_EQ(kRequestId, options->request_id);
  EXPECT_EQ(kFileHandle, options->open_request_id);
  EXPECT_EQ(kOffset, static_cast<double>(options->offset));
  EXPECT_EQ(kLength, options->length);
}

TEST_F(FileSystemProviderOperationsReadFileTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/false);
  CallbackLogger callback_logger;

  ReadFile read_file(&dispatcher, file_system_info_, kFileHandle,
                     io_buffer_.get(), kOffset, kLength,
                     base::BindRepeating(&CallbackLogger::OnReadFile,
                                         base::Unretained(&callback_logger)));

  EXPECT_FALSE(read_file.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsReadFileTest, OnSuccess) {
  using extensions::api::file_system_provider_internal::
      ReadFileRequestedSuccess::Params;

  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  ReadFile read_file(&dispatcher, file_system_info_, kFileHandle,
                     io_buffer_.get(), kOffset, kLength,
                     base::BindRepeating(&CallbackLogger::OnReadFile,
                                         base::Unretained(&callback_logger)));

  EXPECT_TRUE(read_file.Execute(kRequestId));

  const std::string data = "ABCDE";
  const bool has_more = false;
  const int execution_time = 0;

  base::Value::List list;
  list.Append(kFileSystemId);
  list.Append(kRequestId);
  list.Append(base::Value(base::as_byte_span(data)));
  list.Append(has_more);
  list.Append(execution_time);

  std::optional<Params> params = Params::Create(std::move(list));
  ASSERT_TRUE(params.has_value());
  RequestValue request_value =
      RequestValue::CreateForReadFileSuccess(std::move(*params));

  read_file.OnSuccess(kRequestId, std::move(request_value), has_more);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(kLength, event->chunk_length());
  EXPECT_FALSE(event->has_more());
  EXPECT_EQ(data, std::string(io_buffer_->data(), kLength));
  EXPECT_EQ(base::File::FILE_OK, event->result());
}

TEST_F(FileSystemProviderOperationsReadFileTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  ReadFile read_file(&dispatcher, file_system_info_, kFileHandle,
                     io_buffer_.get(), kOffset, kLength,
                     base::BindRepeating(&CallbackLogger::OnReadFile,
                                         base::Unretained(&callback_logger)));

  EXPECT_TRUE(read_file.Execute(kRequestId));

  read_file.OnError(kRequestId, RequestValue(),
                    base::File::FILE_ERROR_TOO_MANY_OPENED);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
}

}  // namespace ash::file_system_provider::operations
