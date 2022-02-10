// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/fusebox_service_provider.h"

#include <sys/stat.h>

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// The fs_context argument may look unused, but we need the BindOnce below to
// keep the reference alive until at least this function gets called back.
void ReplyToStat(scoped_refptr<storage::FileSystemContext> fs_context,
                 dbus::MethodCall* method,
                 dbus::ExportedObject::ResponseSender sender,
                 base::File::Error error_code,
                 const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));
  mode_t mode = info.is_directory ? S_IFDIR : S_IFREG;
  writer.AppendInt32(mode);
  writer.AppendInt64(info.size);
  writer.AppendDouble(info.last_accessed.ToDoubleT());
  writer.AppendDouble(info.last_modified.ToDoubleT());
  writer.AppendDouble(info.creation_time.ToDoubleT());

  std::move(sender).Run(std::move(response));
}

}  // namespace

FuseBoxServiceProvider::ParseResult::ParseResult(
    base::File::Error error_code_arg)
    : error_code(error_code_arg), fs_context(), fs_url() {}

FuseBoxServiceProvider::ParseResult::ParseResult(
    scoped_refptr<storage::FileSystemContext> fs_context_arg,
    storage::FileSystemURL fs_url_arg)
    : error_code(base::File::Error::FILE_OK),
      fs_context(std::move(fs_context_arg)),
      fs_url(std::move(fs_url_arg)) {}

FuseBoxServiceProvider::ParseResult::~ParseResult() = default;

FuseBoxServiceProvider::FuseBoxServiceProvider()
    : enabled_(ash::features::IsFileManagerFuseBoxEnabled()) {}

FuseBoxServiceProvider::~FuseBoxServiceProvider() = default;

void FuseBoxServiceProvider::Start(scoped_refptr<dbus::ExportedObject> object) {
  object->ExportMethod(
      fusebox::kFuseBoxServiceInterface, fusebox::kStatMethod,
      base::BindRepeating(&FuseBoxServiceProvider::Stat,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce([](const std::string& interface_name,
                        const std::string& method_name, bool success) {
        LOG_IF(ERROR, !success)
            << "Failed to export " << interface_name << "." << method_name;
      }));
}

FuseBoxServiceProvider::ParseResult
FuseBoxServiceProvider::ParseCommonDBusMethodArguments(
    dbus::MessageReader* reader) {
  if (!enabled_) {
    LOG(ERROR) << "Not enabled";
    return ParseResult(base::File::Error::FILE_ERROR_FAILED);
  }

  scoped_refptr<storage::FileSystemContext> fs_context =
      file_manager::util::GetFileManagerFileSystemContext(
          ProfileManager::GetActiveUserProfile());
  if (!fs_context) {
    LOG(ERROR) << "No FileSystemContext";
    return ParseResult(base::File::Error::FILE_ERROR_FAILED);
  }

  std::string fs_url_as_string;
  if (!reader->PopString(&fs_url_as_string)) {
    LOG(ERROR) << "No FileSystemURL";
    return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
  }

  storage::FileSystemURL fs_url =
      fs_context->CrackURLInFirstPartyContext(GURL(fs_url_as_string));
  if (!fs_url.is_valid()) {
    LOG(ERROR) << "Invalid FileSystemURL " << fs_url_as_string;
    return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
  } else if (!fs_context->external_backend()->CanHandleType(fs_url.type())) {
    LOG(ERROR) << "Backend cannot handle "
               << storage::GetFileSystemTypeString(fs_url.type());
    return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
  }

  return ParseResult(std::move(fs_context), std::move(fs_url));
}

void FuseBoxServiceProvider::Stat(dbus::MethodCall* method,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);


  dbus::MessageReader reader(method);
  auto common = ParseCommonDBusMethodArguments(&reader);
  if (common.error_code != base::File::Error::FILE_OK) {
    base::File::Info info;
    ReplyToStat(std::move(common.fs_context), method, std::move(sender),
                common.error_code, info);
    return;
  }

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED;

  auto callback =
      base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                         base::BindOnce(&ReplyToStat, common.fs_context, method,
                                        std::move(sender)));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::GetMetadata),
          // Unretained is safe: common.fs_context owns its operation_runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, metadata_fields, std::move(callback)));
}

}  // namespace ash
