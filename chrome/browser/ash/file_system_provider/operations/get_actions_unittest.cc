// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/get_actions.h"

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
const base::FilePath::CharType kFilePath[] = FILE_PATH_LITERAL("/file");

// Callback invocation logger. Acts as a fileapi end-point.
class CallbackLogger {
 public:
  class Event {
   public:
    Event(const Actions& actions, base::File::Error result)
        : actions_(actions), result_(result) {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    virtual ~Event() = default;

    const Actions& actions() const { return actions_; }
    base::File::Error result() const { return result_; }

   private:
    Actions actions_;
    base::File::Error result_;
  };

  CallbackLogger() = default;

  CallbackLogger(const CallbackLogger&) = delete;
  CallbackLogger& operator=(const CallbackLogger&) = delete;

  virtual ~CallbackLogger() = default;

  void OnGetActions(const Actions& actions, base::File::Error result) {
    events_.push_back(std::make_unique<Event>(actions, result));
  }

  const std::vector<std::unique_ptr<Event>>& events() const { return events_; }

 private:
  std::vector<std::unique_ptr<Event>> events_;
};

// Returns the request value as |result| in case of successful parse.
void CreateRequestValueFromJSON(const std::string& json, RequestValue* result) {
  using extensions::api::file_system_provider_internal::
      GetActionsRequestedSuccess::Params;

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(json);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;

  ASSERT_TRUE(parsed_json->is_list());
  std::optional<Params> params = Params::Create(parsed_json->GetList());
  ASSERT_TRUE(params.has_value());
  *result = RequestValue::CreateForGetActionsSuccess(std::move(*params));
  ASSERT_TRUE(result->is_valid());
}

}  // namespace

class FileSystemProviderOperationsGetActionsTest : public testing::Test {
 protected:
  FileSystemProviderOperationsGetActionsTest() = default;
  ~FileSystemProviderOperationsGetActionsTest() override = default;

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, MountOptions(kFileSystemId, /*display_name=*/""),
        base::FilePath(), /*configurable=*/false, /*watchable=*/true,
        extensions::SOURCE_FILE, IconSet());
    entry_paths_.clear();
    entry_paths_.emplace_back(kDirectoryPath);
    entry_paths_.emplace_back(kFilePath);
  }

  ProvidedFileSystemInfo file_system_info_;
  std::vector<base::FilePath> entry_paths_;
};

TEST_F(FileSystemProviderOperationsGetActionsTest, Execute) {
  using extensions::api::file_system_provider::GetActionsRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  GetActions get_actions(&dispatcher, file_system_info_, entry_paths_,
                         base::BindOnce(&CallbackLogger::OnGetActions,
                                        base::Unretained(&callback_logger)));

  EXPECT_TRUE(get_actions.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(
      extensions::api::file_system_provider::OnGetActionsRequested::kEventName,
      event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  auto options =
      GetActionsRequestedOptions::FromValue(options_as_value->GetDict());
  ASSERT_TRUE(options);
  EXPECT_EQ(kFileSystemId, options->file_system_id);
  EXPECT_EQ(kRequestId, options->request_id);
  ASSERT_EQ(entry_paths_.size(), options->entry_paths.size());
  EXPECT_EQ(entry_paths_[0].value(), options->entry_paths[0]);
  EXPECT_EQ(entry_paths_[1].value(), options->entry_paths[1]);
}

TEST_F(FileSystemProviderOperationsGetActionsTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/false);
  CallbackLogger callback_logger;

  GetActions get_actions(&dispatcher, file_system_info_, entry_paths_,
                         base::BindOnce(&CallbackLogger::OnGetActions,
                                        base::Unretained(&callback_logger)));

  EXPECT_FALSE(get_actions.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsGetActionsTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  GetActions get_actions(&dispatcher, file_system_info_, entry_paths_,
                         base::BindOnce(&CallbackLogger::OnGetActions,
                                        base::Unretained(&callback_logger)));

  EXPECT_TRUE(get_actions.Execute(kRequestId));

  // Sample input as JSON. Keep in sync with file_system_provider_api.idl.
  // As for now, it is impossible to create *::Params class directly, not from
  // base::Value.
  const std::string input =
      "[\n"
      "  \"testing-file-system\",\n"  // kFileSystemId
      "  2,\n"                        // kRequestId
      "  [\n"
      "    {\n"
      "      \"id\": \"SAVE_FOR_OFFLINE\"\n"
      "    },\n"
      "    {\n"
      "      \"id\": \"OFFLINE_NOT_NECESSARY\",\n"
      "      \"title\": \"Ignored title.\"\n"
      "    },\n"
      "    {\n"
      "      \"id\": \"SomeCustomActionId\",\n"
      "      \"title\": \"Custom action.\"\n"
      "    }\n"
      "  ],\n"
      "  0\n"  // execution_time
      "]\n";
  RequestValue request_value;
  ASSERT_NO_FATAL_FAILURE(CreateRequestValueFromJSON(input, &request_value));

  const bool has_more = false;
  get_actions.OnSuccess(kRequestId, std::move(request_value), has_more);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(base::File::FILE_OK, event->result());

  ASSERT_EQ(3u, event->actions().size());
  const Action action_share = event->actions()[0];
  EXPECT_EQ("SAVE_FOR_OFFLINE", action_share.id);
  EXPECT_EQ("", action_share.title);

  const Action action_pin_toggle = event->actions()[1];
  EXPECT_EQ("OFFLINE_NOT_NECESSARY", action_pin_toggle.id);
  EXPECT_EQ("Ignored title.", action_pin_toggle.title);

  const Action action_custom = event->actions()[2];
  EXPECT_EQ("SomeCustomActionId", action_custom.id);
  EXPECT_EQ("Custom action.", action_custom.title);
}

TEST_F(FileSystemProviderOperationsGetActionsTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  CallbackLogger callback_logger;

  GetActions get_actions(&dispatcher, file_system_info_, entry_paths_,
                         base::BindOnce(&CallbackLogger::OnGetActions,
                                        base::Unretained(&callback_logger)));

  EXPECT_TRUE(get_actions.Execute(kRequestId));

  get_actions.OnError(kRequestId, RequestValue(),
                      base::File::FILE_ERROR_TOO_MANY_OPENED);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0].get();
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
  ASSERT_EQ(0u, event->actions().size());
}

}  // namespace ash::file_system_provider::operations
