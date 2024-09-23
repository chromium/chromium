// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/read_directory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/operations/get_metadata.h"
#include "chrome/browser/ash/file_system_provider/operations/test_util.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/file_system/async_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider::operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const base::FilePath::CharType kDirectoryPath[] =
    FILE_PATH_LITERAL("/directory");

// Callback invocation logger. Acts as a fileapi end-point.
class CallbackLogger {
 public:
  class Event {
   public:
    Event(base::File::Error result,
          storage::AsyncFileUtil::EntryList entry_list,
          bool has_more)
        : result_(result),
          entry_list_(std::move(entry_list)),
          has_more_(has_more) {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    virtual ~Event() = default;

    base::File::Error result() { return result_; }
    const storage::AsyncFileUtil::EntryList& entry_list() {
      return entry_list_;
    }
    bool has_more() { return has_more_; }

   private:
    base::File::Error result_;
    storage::AsyncFileUtil::EntryList entry_list_;
    bool has_more_;
  };

  CallbackLogger() = default;

  CallbackLogger(const CallbackLogger&) = delete;
  CallbackLogger& operator=(const CallbackLogger&) = delete;

  virtual ~CallbackLogger() = default;

  void OnReadDirectory(base::File::Error result,
                       storage::AsyncFileUtil::EntryList entry_list,
                       bool has_more) {
    events_.push_back(
        std::make_unique<Event>(result, std::move(entry_list), has_more));
  }

  std::vector<std::unique_ptr<Event>>& events() { return events_; }

 private:
  std::vector<std::unique_ptr<Event>> events_;
};

// Returns the request value as |result| in case of successful parse.
void CreateRequestValueFromJSON(const std::string& json, RequestValue* result) {
  using extensions::api::file_system_provider_internal::
      ReadDirectoryRequestedSuccess::Params;

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(json);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;

  ASSERT_TRUE(parsed_json->is_list());
  std::optional<Params> params = Params::Create(parsed_json->GetList());
  ASSERT_TRUE(params.has_value());
  *result = RequestValue::CreateForReadDirectorySuccess(std::move(*params));
  ASSERT_TRUE(result->is_valid());
}

}  // namespace

class FileSystemProviderOperationsReadDirectoryTest : public testing::Test {
 protected:
  FileSystemProviderOperationsReadDirectoryTest() = default;
  ~FileSystemProviderOperationsReadDirectoryTest() override = default;

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, MountOptions(kFileSystemId, /*display_name=*/""),
        base::FilePath(), /*configurable=*/false, /*watchable=*/true,
        extensions::SOURCE_FILE, IconSet());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsReadDirectoryTest, Execute) {
  using extensions::api::file_system_provider::ReadDirectoryRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(
      &dispatcher, file_system_info_, base::FilePath(kDirectoryPath),
      base::BindRepeating(&CallbackLogger::OnReadDirectory,
                          base::Unretained(&callback_logger)));

  EXPECT_TRUE(read_directory.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(extensions::api::file_system_provider::OnReadDirectoryRequested::
                kEventName,
            event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  auto options =
      ReadDirectoryRequestedOptions::FromValue(options_as_value->GetDict());
  ASSERT_TRUE(options);
  EXPECT_EQ(kFileSystemId, options->file_system_id);
  EXPECT_EQ(kRequestId, options->request_id);
  EXPECT_EQ(kDirectoryPath, options->directory_path);
}

TEST_F(FileSystemProviderOperationsReadDirectoryTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/false);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(
      &dispatcher, file_system_info_, base::FilePath(kDirectoryPath),
      base::BindRepeating(&CallbackLogger::OnReadDirectory,
                          base::Unretained(&callback_logger)));

  EXPECT_FALSE(read_directory.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsReadDirectoryTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(
      &dispatcher, file_system_info_, base::FilePath(kDirectoryPath),
      base::BindRepeating(&CallbackLogger::OnReadDirectory,
                          base::Unretained(&callback_logger)));

  EXPECT_TRUE(read_directory.Execute(kRequestId));

  // Sample input as JSON. Keep in sync with file_system_provider_api.idl.
  // As for now, it is impossible to create *::Params class directly, not from
  // base::Value.
  const std::string input =
      "[\n"
      "  \"testing-file-system\",\n"  // kFileSystemId
      "  2,\n"                        // kRequestId
      "  [\n"
      "    {\n"
      "      \"isDirectory\": false,\n"
      "      \"name\": \"blueberries.txt\"\n"
      "    }\n"
      "  ],\n"
      "  false,\n"  // has_more
      "  0\n"       // execution_time
      "]\n";
  RequestValue request_value;
  ASSERT_NO_FATAL_FAILURE(CreateRequestValueFromJSON(input, &request_value));

  const bool has_more = false;
  read_directory.OnSuccess(kRequestId, std::move(request_value), has_more);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(base::File::FILE_OK, event->result());

  ASSERT_EQ(1u, event->entry_list().size());
  const filesystem::mojom::DirectoryEntry entry = event->entry_list()[0];
  EXPECT_EQ(entry.type, filesystem::mojom::FsFileType::REGULAR_FILE);
  EXPECT_EQ("blueberries.txt", entry.name.value());
}

TEST_F(FileSystemProviderOperationsReadDirectoryTest,
       OnSuccess_InvalidMetadata) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(
      &dispatcher, file_system_info_, base::FilePath(kDirectoryPath),
      base::BindRepeating(&CallbackLogger::OnReadDirectory,
                          base::Unretained(&callback_logger)));

  EXPECT_TRUE(read_directory.Execute(kRequestId));

  // Sample input as JSON. Keep in sync with file_system_provider_api.idl.
  // As for now, it is impossible to create *::Params class directly, not from
  // base::Value.
  const std::string input =
      "[\n"
      "  \"testing-file-system\",\n"  // kFileSystemId
      "  2,\n"                        // kRequestId
      "  [\n"
      "    {\n"
      "      \"isDirectory\": false,\n"
      "      \"name\": \"blue/berries.txt\"\n"
      "    }\n"
      "  ],\n"
      "  false,\n"  // has_more
      "  0\n"       // execution_time
      "]\n";
  RequestValue request_value;
  ASSERT_NO_FATAL_FAILURE(CreateRequestValueFromJSON(input, &request_value));

  const bool has_more = false;
  read_directory.OnSuccess(kRequestId, std::move(request_value), has_more);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(base::File::FILE_ERROR_IO, event->result());

  EXPECT_EQ(0u, event->entry_list().size());
}

TEST_F(FileSystemProviderOperationsReadDirectoryTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(
      &dispatcher, file_system_info_, base::FilePath(kDirectoryPath),
      base::BindRepeating(&CallbackLogger::OnReadDirectory,
                          base::Unretained(&callback_logger)));

  EXPECT_TRUE(read_directory.Execute(kRequestId));

  read_directory.OnError(kRequestId, RequestValue(),
                         base::File::FILE_ERROR_TOO_MANY_OPENED);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
  ASSERT_EQ(0u, event->entry_list().size());
}

}  // namespace ash::file_system_provider::operations
