// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/ash/file_system_provider/operation_request_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/file_system_provider/service_worker_lifetime_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/provider_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace extensions {
namespace {

ash::file_system_provider::ProvidedFileSystemInterface* GetProvidedFileSystem(
    content::BrowserContext* browser_context,
    const ash::file_system_provider::ProviderId& provider_id,
    const std::string& file_system_id) {
  auto& service =
      CHECK_DEREF(ash::file_system_provider::Service::Get(browser_context));
  return service.GetProvidedFileSystem(provider_id, file_system_id);
}

api::file_system_provider::FileSystemInfo ConvertFileSystemToExtension(
    ash::file_system_provider::ProvidedFileSystemInterface& file_system) {
  const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info =
      file_system.GetFileSystemInfo();

  api::file_system_provider::FileSystemInfo item;
  item.file_system_id = file_system_info.file_system_id();
  item.display_name = file_system_info.display_name();
  item.writable = file_system_info.writable();
  item.opened_files_limit = file_system_info.opened_files_limit();
  item.supports_notify_tag = file_system_info.supports_notify_tag();

  if (file_system_info.watchable()) {
    for (const auto& watcher : *file_system.GetWatchers()) {
      api::file_system_provider::Watcher watcher_item;
      watcher_item.entry_path = watcher.second.entry_path.value();
      watcher_item.recursive = watcher.second.recursive;
      if (!watcher.second.last_tag.empty()) {
        watcher_item.last_tag = watcher.second.last_tag;
      }
      item.watchers.push_back(std::move(watcher_item));
    }
  }

  for (const auto& opened_file : file_system.GetOpenedFiles()) {
    api::file_system_provider::OpenedFile opened_file_item;
    opened_file_item.open_request_id = opened_file.first;
    opened_file_item.file_path = opened_file.second.file_path.value();
    switch (opened_file.second.mode) {
      case ash::file_system_provider::OPEN_FILE_MODE_READ:
        opened_file_item.mode =
            extensions::api::file_system_provider::OpenFileMode::kRead;
        break;
      case ash::file_system_provider::OPEN_FILE_MODE_WRITE:
        opened_file_item.mode =
            extensions::api::file_system_provider::OpenFileMode::kWrite;
        break;
    }
    item.opened_files.push_back(std::move(opened_file_item));
  }

  return item;
}

// Converts the change type from the IDL type to a storage type. |changed_type|
// must be specified (not CHANGE_TYPE_NONE).
storage::WatcherManager::ChangeType ParseChangeType(
    const api::file_system_provider::ChangeType& change_type) {
  switch (change_type) {
    case api::file_system_provider::ChangeType::kChanged:
      return storage::WatcherManager::CHANGED;
    case api::file_system_provider::ChangeType::kDeleted:
      return storage::WatcherManager::DELETED;
    default:
      break;
  }
  NOTREACHED();
}

std::unique_ptr<ash::file_system_provider::CloudFileInfo> ParseCloudFileInfo(
    const std::optional<api::file_system_provider::CloudFileInfo>&
        cloud_file_info) {
  if (!cloud_file_info.has_value()) {
    return nullptr;
  }
  if (!cloud_file_info->version_tag.has_value()) {
    return nullptr;
  }
  return std::make_unique<ash::file_system_provider::CloudFileInfo>(
      cloud_file_info->version_tag.value());
}

ash::file_system_provider::ProvidedFileSystemObserver::Change ParseChange(
    const api::file_system_provider::Change& change) {
  return ash::file_system_provider::ProvidedFileSystemObserver::Change(
      base::FilePath::FromUTF8Unsafe(change.entry_path),
      ParseChangeType(change.change_type),
      ParseCloudFileInfo(change.cloud_file_info));
}

std::unique_ptr<ash::file_system_provider::ProvidedFileSystemObserver::Changes>
ParseChanges(
    const std::optional<std::vector<api::file_system_provider::Change>>&
        changes) {
  auto results = std::make_unique<
      ash::file_system_provider::ProvidedFileSystemObserver::Changes>();
  if (changes.has_value()) {
    for (const auto& change : *changes) {
      results->push_back(ParseChange(change));
    }
  }
  return results;
}

std::string ForwardOperationResponseImpl(
    ash::file_system_provider::RequestManager& manager,
    int64_t request_id,
    const ash::file_system_provider::RequestValue& value,
    std::variant<bool /*has_more*/, base::File::Error /*error*/> arg) {
  const base::File::Error result = std::visit(
      absl::Overload{[&](bool has_more) {
                       return manager.FulfillRequest(request_id, value,
                                                     has_more);
                     },
                     [&](base::File::Error error) {
                       return manager.RejectRequest(request_id, value, error);
                     }},
      arg);
  if (result != base::File::FILE_OK) {
    return extensions::FileErrorToString(result);
  }
  return "";
}

}  // namespace

std::string FileSystemProviderBase::GetProviderId() const {
  // Terminal app is the only non-extension to use fsp.
  if (!extension()) {
    CHECK(url::IsSameOriginWith(source_url(),
                                GURL(chrome::kChromeUIUntrustedTerminalURL)));
    return guest_os::kTerminalSystemAppId;
  }
  return extension_id();
}

void FileSystemProviderBase::RespondWithError(const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }
}

ExtensionFunction::ResponseAction FileSystemProviderMountFunction::Run() {
  using api::file_system_provider::Mount::Params;
  const std::optional<Params> params(Params::Create(args()));
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
  if (params->options.opened_files_limit &&
      *params->options.opened_files_limit < 0) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION)));
  }

  ash::file_system_provider::MountOptions options;
  options.file_system_id = params->options.file_system_id;
  options.display_name = params->options.display_name;
  options.writable = params->options.writable.value_or(false);
  options.opened_files_limit = params->options.opened_files_limit.value_or(0);
  options.supports_notify_tag =
      params->options.supports_notify_tag.value_or(false);
  options.persistent = params->options.persistent.value_or(true);

  auto& service =
      CHECK_DEREF(ash::file_system_provider::Service::Get(browser_context()));
  const base::File::Error result = service.MountFileSystem(
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          GetProviderId()),
      options);

  if (result != base::File::FILE_OK) {
    return RespondNow(Error(extensions::FileErrorToString(result)));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileSystemProviderUnmountFunction::Run() {
  using api::file_system_provider::Unmount::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto& service =
      CHECK_DEREF(ash::file_system_provider::Service::Get(browser_context()));
  const base::File::Error result = service.UnmountFileSystem(
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          GetProviderId()),
      params->options.file_system_id,
      ash::file_system_provider::Service::UNMOUNT_REASON_USER);
  if (result != base::File::FILE_OK) {
    return RespondNow(Error(extensions::FileErrorToString(result)));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileSystemProviderGetAllFunction::Run() {
  auto& service =
      CHECK_DEREF(ash::file_system_provider::Service::Get(browser_context()));
  auto provider_id =
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          GetProviderId());

  std::vector<api::file_system_provider::FileSystemInfo> items;
  for (const auto& file_system_info :
       service.GetProvidedFileSystemInfoList(provider_id)) {
    ash::file_system_provider::ProvidedFileSystemInterface* const file_system =
        service.GetProvidedFileSystem(file_system_info.provider_id(),
                                      file_system_info.file_system_id());
    items.push_back(ConvertFileSystemToExtension(CHECK_DEREF(file_system)));
  }
  return RespondNow(
      ArgumentList(api::file_system_provider::GetAll::Results::Create(items)));
}

ExtensionFunction::ResponseAction FileSystemProviderGetFunction::Run() {
  using api::file_system_provider::Get::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* file_system = GetProvidedFileSystem(
      browser_context(),
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          GetProviderId()),
      params->file_system_id);
  if (!file_system) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND)));
  }

  auto result = ConvertFileSystemToExtension(*file_system);
  return RespondNow(ArgumentList(
      api::file_system_provider::Get::Results::Create(std::move(result))));
}

ExtensionFunction::ResponseAction FileSystemProviderNotifyFunction::Run() {
  using api::file_system_provider::Notify::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* file_system = GetProvidedFileSystem(
      browser_context(),
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          GetProviderId()),
      params->options.file_system_id);
  if (!file_system) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND)));
  }

  file_system->Notify(
      base::FilePath::FromUTF8Unsafe(params->options.observed_path),
      params->options.recursive, ParseChangeType(params->options.change_type),
      ParseChanges(params->options.changes),
      params->options.tag.value_or(std::string()),
      base::BindOnce(&FileSystemProviderNotifyFunction::OnNotifyCompleted,
                     this));
  return RespondLater();
}

void FileSystemProviderNotifyFunction::OnNotifyCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error(FileErrorToString(result)));
    return;
  }

  Respond(NoArguments());
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalRespondToMountRequestFunction::Run() {
  using api::file_system_provider_internal::RespondToMountRequest::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* profile = Profile::FromBrowserContext(browser_context());
  auto* sw_lifetime_manager =
      ash::file_system_provider::ServiceWorkerLifetimeManager::Get(profile);
  sw_lifetime_manager->FinishRequest(
      {extension_id(), /*file_system_id=*/std::string(), params->request_id});

  auto& service = CHECK_DEREF(ash::file_system_provider::Service::Get(profile));
  auto* provider = service.GetProvider(
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          extension_id()));
  if (!provider) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND)));
  }

  base::File::Error mount_error =
      extensions::ProviderErrorToFileError(params->error);

  auto arg =
      mount_error == base::File::FILE_OK
          ? std::variant<bool /*has_more*/, base::File::Error /*error*/>(false)
          : mount_error;

  std::string error = ForwardOperationResponseImpl(
      CHECK_DEREF(provider->GetRequestManager()), params->request_id,
      ash::file_system_provider::RequestValue(), arg);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileSystemProviderInternal::ForwardOperationResult(
    const std::string& file_system_id,
    int64_t request_id,
    const ash::file_system_provider::RequestValue& value,
    std::variant<bool /*has_more*/, base::File::Error /*error*/> arg) {
  auto* profile = Profile::FromBrowserContext(browser_context());
  auto* sw_lifetime_manager =
      ash::file_system_provider::ServiceWorkerLifetimeManager::Get(profile);
  auto provider_id = GetProviderId();
  sw_lifetime_manager->FinishRequest({provider_id, file_system_id, request_id});

  auto* file_system = GetProvidedFileSystem(
      browser_context(),
      ash::file_system_provider::ProviderId::CreateFromExtensionId(provider_id),
      file_system_id);
  if (!file_system) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND)));
  }

  std::string error = ForwardOperationResponseImpl(
      CHECK_DEREF(file_system->GetRequestManager()), request_id, value, arg);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalUnmountRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::UnmountRequestedSuccess::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string file_system_id = params->file_system_id;
  int request_id = params->request_id;
  return ForwardOperationResult(
      file_system_id, request_id,
      ash::file_system_provider::RequestValue::CreateForUnmountSuccess(
          std::move(*params)),
      /*has_more=*/false);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalGetMetadataRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::GetMetadataRequestedSuccess::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string file_system_id = params->file_system_id;
  int request_id = params->request_id;
  return ForwardOperationResult(
      file_system_id, request_id,
      ash::file_system_provider::RequestValue::CreateForGetMetadataSuccess(
          std::move(*params)),
      /*has_more=*/false);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalGetActionsRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::GetActionsRequestedSuccess::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string file_system_id = params->file_system_id;
  int request_id = params->request_id;
  return ForwardOperationResult(
      file_system_id, request_id,
      ash::file_system_provider::RequestValue::CreateForGetActionsSuccess(
          std::move(*params)),
      /*has_more=*/false);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalReadDirectoryRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::ReadDirectoryRequestedSuccess::
      Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string file_system_id = params->file_system_id;
  int request_id = params->request_id;
  bool has_more = params->has_more;
  return ForwardOperationResult(
      file_system_id, request_id,
      ash::file_system_provider::RequestValue::CreateForReadDirectorySuccess(
          std::move(*params)),
      has_more);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalReadFileRequestedSuccessFunction::Run() {
  TRACE_EVENT0("file_system_provider", "ReadFileRequestedSuccess");
  using api::file_system_provider_internal::ReadFileRequestedSuccess::Params;

  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string file_system_id = params->file_system_id;
  int request_id = params->request_id;
  bool has_more = params->has_more;
  return ForwardOperationResult(
      file_system_id, request_id,
      ash::file_system_provider::RequestValue::CreateForReadFileSuccess(
          std::move(*params)),
      has_more);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalOpenFileRequestedSuccessFunction::Run() {
  TRACE_EVENT0("file_system_provider", "OpenFileRequestedSuccess");
  using api::file_system_provider_internal::OpenFileRequestedSuccess::Params;

  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string file_system_id = params->file_system_id;
  int request_id = params->request_id;
  return ForwardOperationResult(
      file_system_id, request_id,
      ash::file_system_provider::RequestValue::CreateForOpenFileSuccess(
          std::move(*params)),
      /*has_more=*/false);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalOperationRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::OperationRequestedSuccess::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string file_system_id = params->file_system_id;
  int request_id = params->request_id;
  return ForwardOperationResult(
      file_system_id, request_id,
      ash::file_system_provider::RequestValue::CreateForOperationSuccess(
          std::move(*params)),
      /*has_more=*/false);
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalOperationRequestedErrorFunction::Run() {
  using api::file_system_provider_internal::OperationRequestedError::Params;
  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->error == api::file_system_provider::ProviderError::kOk) {
    // It's incorrect to pass OK as an error code.
    return ValidationFailure(this);
  }

  std::string file_system_id = params->file_system_id;
  int request_id = params->request_id;
  base::File::Error operation_error =
      extensions::ProviderErrorToFileError(params->error);
  return ForwardOperationResult(
      file_system_id, request_id,
      ash::file_system_provider::RequestValue::CreateForOperationError(
          std::move(*params)),
      operation_error);
}

}  // namespace extensions
