// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "storage/browser/file_system/watcher_manager.h"

using chromeos::file_system_provider::MountOptions;
using chromeos::file_system_provider::OpenedFiles;
using chromeos::file_system_provider::ProvidedFileSystemInfo;
using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::ProvidedFileSystemObserver;
using chromeos::file_system_provider::ProviderId;
using chromeos::file_system_provider::RequestValue;
using chromeos::file_system_provider::Service;
using chromeos::file_system_provider::Watchers;

namespace extensions {
namespace {

// Converts the change type from the IDL type to a native type. |changed_type|
// must be specified (not CHANGE_TYPE_NONE).
storage::WatcherManager::ChangeType ParseChangeType(
    const api::file_system_provider::ChangeType& change_type) {
  switch (change_type) {
    case api::file_system_provider::CHANGE_TYPE_CHANGED:
      return storage::WatcherManager::CHANGED;
    case api::file_system_provider::CHANGE_TYPE_DELETED:
      return storage::WatcherManager::DELETED;
    default:
      break;
  }
  NOTREACHED();
  return storage::WatcherManager::CHANGED;
}

// Convert the change from the IDL type to a native type. The reason IDL types
// are not used is since they are imperfect, eg. paths are stored as strings.
ProvidedFileSystemObserver::Change ParseChange(
    const api::file_system_provider::Change& change) {
  ProvidedFileSystemObserver::Change result;
  result.entry_path = base::FilePath::FromUTF8Unsafe(change.entry_path);
  result.change_type = ParseChangeType(change.change_type);
  return result;
}

// Converts a list of child changes from the IDL type to a native type.
std::unique_ptr<ProvidedFileSystemObserver::Changes> ParseChanges(
    const std::vector<api::file_system_provider::Change>& changes) {
  std::unique_ptr<ProvidedFileSystemObserver::Changes> results(
      new ProvidedFileSystemObserver::Changes);
  for (const auto& change : changes) {
    results->push_back(ParseChange(change));
  }
  return results;
}

// Fills the IDL's FileSystemInfo with FSP's ProvidedFileSystemInfo and
// Watchers.
void FillFileSystemInfo(const ProvidedFileSystemInfo& file_system_info,
                        const Watchers& watchers,
                        const OpenedFiles& opened_files,
                        api::file_system_provider::FileSystemInfo* output) {
  using api::file_system_provider::Watcher;
  using api::file_system_provider::OpenedFile;

  output->file_system_id = file_system_info.file_system_id();
  output->display_name = file_system_info.display_name();
  output->writable = file_system_info.writable();
  output->opened_files_limit = file_system_info.opened_files_limit();

  for (const auto& watcher : watchers) {
    Watcher watcher_item;
    watcher_item.entry_path = watcher.second.entry_path.value();
    watcher_item.recursive = watcher.second.recursive;
    if (!watcher.second.last_tag.empty())
      watcher_item.last_tag.reset(new std::string(watcher.second.last_tag));
    output->watchers.push_back(std::move(watcher_item));
  }

  for (const auto& opened_file : opened_files) {
    OpenedFile opened_file_item;
    opened_file_item.open_request_id = opened_file.first;
    opened_file_item.file_path = opened_file.second.file_path.value();
    switch (opened_file.second.mode) {
      case chromeos::file_system_provider::OPEN_FILE_MODE_READ:
        opened_file_item.mode =
            extensions::api::file_system_provider::OPEN_FILE_MODE_READ;
        break;
      case chromeos::file_system_provider::OPEN_FILE_MODE_WRITE:
        opened_file_item.mode =
            extensions::api::file_system_provider::OPEN_FILE_MODE_WRITE;
        break;
    }
    output->opened_files.push_back(std::move(opened_file_item));
  }
}

}  // namespace

ExtensionFunction::ResponseAction FileSystemProviderMountFunction::Run() {
  using api::file_system_provider::Mount::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // It's an error if the file system Id is empty.
  if (params->options.file_system_id.empty()) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION)));
  }

  // It's an error if the display name is empty.
  if (params->options.display_name.empty()) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION)));
  }

  // If the opened files limit is set, then it must be larger or equal than 0.
  if (params->options.opened_files_limit.get() &&
      *params->options.opened_files_limit.get() < 0) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION)));
  }

  Service* const service =
      Service::Get(Profile::FromBrowserContext(browser_context()));
  DCHECK(service);

  MountOptions options;
  options.file_system_id = params->options.file_system_id;
  options.display_name = params->options.display_name;
  options.writable = params->options.writable != nullptr;
  options.opened_files_limit = params->options.opened_files_limit.get()
                                   ? *params->options.opened_files_limit.get()
                                   : 0;
  options.supports_notify_tag = params->options.supports_notify_tag != nullptr;
  options.persistent = params->options.persistent.get()
                           ? *params->options.persistent.get()
                           : true;

  const base::File::Error result = service->MountFileSystem(
      ProviderId::CreateFromExtensionId(extension_id()), options);
  if (result != base::File::FILE_OK)
    return RespondNow(Error(FileErrorToString(result)));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileSystemProviderUnmountFunction::Run() {
  using api::file_system_provider::Unmount::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* const service =
      Service::Get(Profile::FromBrowserContext(browser_context()));
  DCHECK(service);

  const base::File::Error result = service->UnmountFileSystem(
      ProviderId::CreateFromExtensionId(extension_id()),
      params->options.file_system_id, Service::UNMOUNT_REASON_USER);
  if (result != base::File::FILE_OK)
    return RespondNow(Error(FileErrorToString(result)));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileSystemProviderGetAllFunction::Run() {
  using api::file_system_provider::FileSystemInfo;
  Service* const service =
      Service::Get(Profile::FromBrowserContext(browser_context()));
  DCHECK(service);

  ProviderId provider_id = ProviderId::CreateFromExtensionId(extension_id());
  const std::vector<ProvidedFileSystemInfo> file_systems =
      service->GetProvidedFileSystemInfoList(provider_id);

  std::vector<FileSystemInfo> items;

  for (const auto& file_system_info : file_systems) {
    FileSystemInfo item;

    chromeos::file_system_provider::ProvidedFileSystemInterface* const
        file_system = service->GetProvidedFileSystem(
            file_system_info.provider_id(), file_system_info.file_system_id());

    DCHECK(file_system);

    FillFileSystemInfo(
        file_system_info,
        file_system_info.watchable() ? *file_system->GetWatchers() : Watchers(),
        file_system->GetOpenedFiles(), &item);

    items.push_back(std::move(item));
  }

  return RespondNow(
      ArgumentList(api::file_system_provider::GetAll::Results::Create(items)));
}

ExtensionFunction::ResponseAction FileSystemProviderGetFunction::Run() {
  using api::file_system_provider::Get::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using api::file_system_provider::FileSystemInfo;
  Service* const service =
      Service::Get(Profile::FromBrowserContext(browser_context()));
  DCHECK(service);

  chromeos::file_system_provider::ProvidedFileSystemInterface* const
      file_system = service->GetProvidedFileSystem(
          ProviderId::CreateFromExtensionId(extension_id()),
          params->file_system_id);

  if (!file_system) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND)));
  }

  FileSystemInfo file_system_info;
  FillFileSystemInfo(file_system->GetFileSystemInfo(),
                     file_system->GetFileSystemInfo().watchable()
                         ? *file_system->GetWatchers()
                         : Watchers(),
                     file_system->GetOpenedFiles(), &file_system_info);
  return RespondNow(ArgumentList(
      api::file_system_provider::Get::Results::Create(file_system_info)));
}

bool FileSystemProviderNotifyFunction::RunAsync() {
  using api::file_system_provider::Notify::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* const service = Service::Get(GetProfile());
  DCHECK(service);

  ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(
          ProviderId::CreateFromExtensionId(extension_id()),
          params->options.file_system_id);
  if (!file_system) {
    SetError(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND));
    return false;
  }

  file_system->Notify(
      base::FilePath::FromUTF8Unsafe(params->options.observed_path),
      params->options.recursive, ParseChangeType(params->options.change_type),
      params->options.changes.get()
          ? ParseChanges(*params->options.changes.get())
          : base::WrapUnique(new ProvidedFileSystemObserver::Changes),
      params->options.tag.get() ? *params->options.tag.get() : "",
      base::Bind(&FileSystemProviderNotifyFunction::OnNotifyCompleted, this));

  return true;
}

void FileSystemProviderNotifyFunction::OnNotifyCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    SetError(FileErrorToString(result));
    SendResponse(false);
    return;
  }

  SendResponse(true);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalUnmountRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::UnmountRequestedSuccess::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return FulfillRequest(
      RequestValue::CreateForUnmountSuccess(std::move(params)),
      false /* has_more */);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalGetMetadataRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::GetMetadataRequestedSuccess::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return FulfillRequest(
      RequestValue::CreateForGetMetadataSuccess(std::move(params)),
      false /* has_more */);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalGetActionsRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::GetActionsRequestedSuccess::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return FulfillRequest(
      RequestValue::CreateForGetActionsSuccess(std::move(params)),
      false /* has_more */);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalReadDirectoryRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::ReadDirectoryRequestedSuccess::
      Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool has_more = params->has_more;
  return FulfillRequest(
      RequestValue::CreateForReadDirectorySuccess(std::move(params)), has_more);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalReadFileRequestedSuccessFunction::Run() {
  TRACE_EVENT0("file_system_provider", "ReadFileRequestedSuccess");
  using api::file_system_provider_internal::ReadFileRequestedSuccess::Params;

  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool has_more = params->has_more;
  return FulfillRequest(
      RequestValue::CreateForReadFileSuccess(std::move(params)), has_more);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalOperationRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::OperationRequestedSuccess::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return FulfillRequest(
      std::unique_ptr<RequestValue>(
          RequestValue::CreateForOperationSuccess(std::move(params))),
      false /* has_more */);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalOperationRequestedErrorFunction::Run() {
  using api::file_system_provider_internal::OperationRequestedError::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->error == api::file_system_provider::PROVIDER_ERROR_OK) {
    // It's incorrect to pass OK as an error code.
    return ValidationFailure(this);
  }

  const base::File::Error error = ProviderErrorToFileError(params->error);
  return RejectRequest(RequestValue::CreateForOperationError(std::move(params)),
                       error);
}

}  // namespace extensions
