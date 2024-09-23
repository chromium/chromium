// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/unmount.h"

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
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider::operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;

}  // namespace

class FileSystemProviderOperationsUnmountTest : public testing::Test {
 protected:
  FileSystemProviderOperationsUnmountTest() = default;
  ~FileSystemProviderOperationsUnmountTest() override = default;

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, MountOptions(kFileSystemId, /*display_name=*/""),
        base::FilePath(), /*configurable=*/false, /*watchable=*/true,
        extensions::SOURCE_FILE, IconSet());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsUnmountTest, Execute) {
  using extensions::api::file_system_provider::UnmountRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  util::StatusCallbackLog callback_log;

  Unmount unmount(&dispatcher, file_system_info_,
                  base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(
      extensions::api::file_system_provider::OnUnmountRequested::kEventName,
      event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  auto options =
      UnmountRequestedOptions::FromValue(options_as_value->GetDict());
  ASSERT_TRUE(options);
  EXPECT_EQ(kFileSystemId, options->file_system_id);
  EXPECT_EQ(kRequestId, options->request_id);
}

TEST_F(FileSystemProviderOperationsUnmountTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/false);
  util::StatusCallbackLog callback_log;

  Unmount unmount(&dispatcher, file_system_info_,
                  base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_FALSE(unmount.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsUnmountTest, OnSuccess) {
  using extensions::api::file_system_provider_internal::
      UnmountRequestedSuccess::Params;

  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  util::StatusCallbackLog callback_log;

  Unmount unmount(&dispatcher, file_system_info_,
                  base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  unmount.OnSuccess(kRequestId, RequestValue(), /*has_more=*/false);
  ASSERT_EQ(1u, callback_log.size());
  base::File::Error event_result = callback_log[0];
  EXPECT_EQ(base::File::FILE_OK, event_result);
}

TEST_F(FileSystemProviderOperationsUnmountTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(/*dispatch_reply=*/true);
  util::StatusCallbackLog callback_log;

  Unmount unmount(&dispatcher, file_system_info_,
                  base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  unmount.OnError(kRequestId, RequestValue(), base::File::FILE_ERROR_NOT_FOUND);
  ASSERT_EQ(1u, callback_log.size());
  base::File::Error event_result = callback_log[0];
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, event_result);
}

}  // namespace ash::file_system_provider::operations
