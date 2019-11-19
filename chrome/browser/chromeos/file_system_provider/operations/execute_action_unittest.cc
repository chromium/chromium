// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/execute_action.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_system_provider/icon_set.h"
#include "chrome/browser/chromeos/file_system_provider/operations/test_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/file_system/async_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const base::FilePath::CharType kDirectoryPath[] =
    FILE_PATH_LITERAL("/kitty/and/puppy/happy");
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("/rabbit/and/bear/happy");
const char kActionId[] = "SHARE";

}  // namespace

class FileSystemProviderOperationsExecuteActionTest : public testing::Test {
 protected:
  FileSystemProviderOperationsExecuteActionTest() {}
  ~FileSystemProviderOperationsExecuteActionTest() override {}

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, MountOptions(kFileSystemId, "" /* display_name */),
        base::FilePath(), false /* configurable */, true /* watchable */,
        extensions::SOURCE_FILE, IconSet());
    entry_paths_.clear();
    entry_paths_.push_back(base::FilePath(kDirectoryPath));
    entry_paths_.push_back(base::FilePath(kFilePath));
  }

  ProvidedFileSystemInfo file_system_info_;
  std::vector<base::FilePath> entry_paths_;
};

TEST_F(FileSystemProviderOperationsExecuteActionTest, Execute) {
  using extensions::api::file_system_provider::ExecuteActionRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  ExecuteAction execute_action(
      NULL, file_system_info_, entry_paths_, kActionId,
      base::Bind(&util::LogStatusCallback, &callback_log));
  execute_action.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(execute_action.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(extensions::api::file_system_provider::OnExecuteActionRequested::
                kEventName,
            event->event_name);
  base::ListValue* event_args = event->event_args.get();
  ASSERT_EQ(1u, event_args->GetSize());

  const base::DictionaryValue* options_as_value = NULL;
  ASSERT_TRUE(event_args->GetDictionary(0, &options_as_value));

  ExecuteActionRequestedOptions options;
  ASSERT_TRUE(
      ExecuteActionRequestedOptions::Populate(*options_as_value, &options));
  EXPECT_EQ(kFileSystemId, options.file_system_id);
  EXPECT_EQ(kRequestId, options.request_id);
  ASSERT_EQ(entry_paths_.size(), options.entry_paths.size());
  EXPECT_EQ(entry_paths_[0].value(), options.entry_paths[0]);
  EXPECT_EQ(entry_paths_[1].value(), options.entry_paths[1]);
  EXPECT_EQ(kActionId, options.action_id);
}

TEST_F(FileSystemProviderOperationsExecuteActionTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  ExecuteAction execute_action(
      NULL, file_system_info_, entry_paths_, kActionId,
      base::Bind(&util::LogStatusCallback, &callback_log));
  execute_action.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(execute_action.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsExecuteActionTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  ExecuteAction execute_action(
      NULL, file_system_info_, entry_paths_, kActionId,
      base::Bind(&util::LogStatusCallback, &callback_log));
  execute_action.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(execute_action.Execute(kRequestId));

  execute_action.OnSuccess(kRequestId,
                           std::unique_ptr<RequestValue>(new RequestValue()),
                           false /* has_more */);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_OK, callback_log[0]);
}

TEST_F(FileSystemProviderOperationsExecuteActionTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  ExecuteAction execute_action(
      NULL, file_system_info_, entry_paths_, kActionId,
      base::Bind(&util::LogStatusCallback, &callback_log));
  execute_action.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(execute_action.Execute(kRequestId));

  execute_action.OnError(kRequestId,
                         std::unique_ptr<RequestValue>(new RequestValue()),
                         base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, callback_log[0]);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
