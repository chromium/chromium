// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"

#include "base/base64.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/file_system_provider/cloud_file_info.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/operation_request_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/provider_function.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/url_constants.h"

using ash::file_system_provider::IconSet;
using ash::file_system_provider::MountOptions;
using ash::file_system_provider::OpenedFiles;
using ash::file_system_provider::ProvidedFileSystemInfo;
using ash::file_system_provider::ProvidedFileSystemInterface;
using ash::file_system_provider::ProvidedFileSystemObserver;
using ash::file_system_provider::ProviderId;
using ash::file_system_provider::RequestValue;
using ash::file_system_provider::Service;
using ash::file_system_provider::Watchers;

namespace crosapi {
namespace {

constexpr char kDeserializationError[] = "deserialization error";

// Either returns a valid request manager for provider-level requests, or else
// an error string.
base::expected<ash::file_system_provider::RequestManager*, std::string>
GetProviderRequestManager(Profile* profile,
                          extensions::ExtensionId extension_id) {
  Service* service = Service::Get(profile);
  if (!service) {
    return base::unexpected("File system provider service not found.");
  }

  ash::file_system_provider::ProviderInterface* provider =
      service->GetProvider(ProviderId::CreateFromExtensionId(extension_id));
  if (!provider) {
    return base::unexpected(
        extensions::FileErrorToString(base::File::FILE_ERROR_NOT_FOUND));
  }

  return provider->GetRequestManager();
}

// Either returns a valid request manager for file system level requests, or
// else an error string.
base::expected<ash::file_system_provider::OperationRequestManager*, std::string>
GetProvidedFileSystemRequestManager(
    Profile* profile,
    const mojom::FileSystemIdPtr& file_system_id) {
  Service* service = Service::Get(profile);
  if (!service) {
    return base::unexpected("File system provider service not found.");
  }

  ProvidedFileSystemInterface* file_system = service->GetProvidedFileSystem(
      ProviderId::CreateFromExtensionId(file_system_id->provider),
      file_system_id->id);
  if (!file_system) {
    return base::unexpected(
        extensions::FileErrorToString(base::File::FILE_ERROR_NOT_FOUND));
  }

  return file_system->GetRequestManager();
}

// Forwards an operation response from an extension to the request manager and
// then returns the error message. Empty string means success.
std::string ForwardOperationResponse(mojom::FileSystemIdPtr file_system_id,
                                     int64_t request_id,
                                     const RequestValue& value,
                                     bool has_more,
                                     Profile* profile) {
  auto manager = GetProvidedFileSystemRequestManager(profile, file_system_id);
  if (!manager.has_value())
    return manager.error();

  const base::File::Error result =
      manager.value()->FulfillRequest(request_id, value, has_more);
  if (result != base::File::FILE_OK) {
    return extensions::FileErrorToString(result);
  }
  return "";
}

// Forwards an operation failure from an extension to the request manager and
// then returns the error message. Empty string means success.
std::string ForwardOperationFailure(mojom::FileSystemIdPtr file_system_id,
                                    int64_t request_id,
                                    const RequestValue& value,
                                    base::File::Error error,
                                    Profile* profile) {
  auto manager = GetProvidedFileSystemRequestManager(profile, file_system_id);
  if (!manager.has_value())
    return manager.error();

  const base::File::Error result =
      manager.value()->RejectRequest(request_id, value, error);
  if (result != base::File::FILE_OK) {
    return extensions::FileErrorToString(result);
  }
  return "";
}

// Convert |result| to a string, empty string for success and invokes
// |callback|.
void RunErrorCallback(base::OnceCallback<void(const std::string&)> callback,
                      const base::File::Error result) {
  std::string error;
  if (result != base::File::FILE_OK) {
    error = extensions::FileErrorToString(result);
  }
  std::move(callback).Run(std::move(error));
}

// Converts from native filesystem watchers to their mojom counterparts
std::vector<crosapi::mojom::FSPWatcherPtr> ConvertWatchersToMojom(
    const Watchers& watchers) {
  std::vector<crosapi::mojom::FSPWatcherPtr> mojom_watchers;
  for (const auto& watcher : watchers) {
    crosapi::mojom::FSPWatcherPtr watcher_item =
        crosapi::mojom::FSPWatcher::New();
    watcher_item->entry_path = watcher.second.entry_path;
    watcher_item->recursive = watcher.second.recursive;
    watcher_item->last_tag = watcher.second.last_tag;
    mojom_watchers.push_back(std::move(watcher_item));
  }
  return mojom_watchers;
}

// Converts from native filesystem opened files to their mojom counterparts
std::vector<crosapi::mojom::OpenedFilePtr> ConvertOpenedFilesToMojom(
    const OpenedFiles& opened_files) {
  std::vector<crosapi::mojom::OpenedFilePtr> mojom_opened_files;
  for (const auto& opened_file : opened_files) {
    crosapi::mojom::OpenedFilePtr opened_file_item =
        crosapi::mojom::OpenedFile::New();
    opened_file_item->open_request_id = opened_file.first;
    opened_file_item->file_path = opened_file.second.file_path.value();
    switch (opened_file.second.mode) {
      case ash::file_system_provider::OPEN_FILE_MODE_READ:
        opened_file_item->mode = crosapi::mojom::OpenFileMode::kRead;
        break;
      case ash::file_system_provider::OPEN_FILE_MODE_WRITE:
        opened_file_item->mode = crosapi::mojom::OpenFileMode::kWrite;
        break;
    }
    mojom_opened_files.push_back(std::move(opened_file_item));
  }
  return mojom_opened_files;
}

// Converts native file system to mojom.
crosapi::mojom::FileSystemInfoPtr ConvertFileSystemToMojom(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info,
    const std::string& provider) {
  Service* const service = Service::Get(profile);
  crosapi::mojom::FileSystemInfoPtr item =
      crosapi::mojom::FileSystemInfo::New();
  item->metadata = crosapi::mojom::FileSystemMetadata::New();
  item->metadata->file_system_id = crosapi::mojom::FileSystemId::New();
  item->metadata->file_system_id->provider = provider;

  ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(file_system_info.provider_id(),
                                     file_system_info.file_system_id());

  DCHECK(file_system);

  item->watchers = ConvertWatchersToMojom(
      file_system_info.watchable() ? *file_system->GetWatchers() : Watchers());
  item->opened_files = ConvertOpenedFilesToMojom(file_system->GetOpenedFiles());
  item->metadata->file_system_id->id = file_system_info.file_system_id();
  item->metadata->display_name = file_system_info.display_name();
  item->metadata->writable = file_system_info.writable();
  item->metadata->opened_files_limit = file_system_info.opened_files_limit();
  item->metadata->supports_notify = file_system_info.supports_notify_tag();

  return item;
}

storage::WatcherManager::ChangeType ParseChangeType(mojom::FSPChangeType type) {
  switch (type) {
    case mojom::FSPChangeType::kChanged:
      return storage::WatcherManager::CHANGED;
    case mojom::FSPChangeType::kDeleted:
      return storage::WatcherManager::DELETED;
  }
}

std::unique_ptr<ash::file_system_provider::CloudFileInfo> ParseCloudFileInfo(
    mojom::CloudFileInfoPtr cloud_file_info) {
  if (cloud_file_info.is_null()) {
    return nullptr;
  }
  if (!cloud_file_info->version_tag.has_value()) {
    return nullptr;
  }
  return std::make_unique<ash::file_system_provider::CloudFileInfo>(
      cloud_file_info->version_tag.value());
}

// Convert the change from the mojom type to a native type.
ProvidedFileSystemObserver::Change ParseChange(mojom::FSPChangePtr change) {
  return ProvidedFileSystemObserver::Change(
      change->path, ParseChangeType(change->type),
      ParseCloudFileInfo(std::move(change->cloud_file_info)));
}

// Converts a list of child changes from the mojom type to a native type.
std::unique_ptr<ProvidedFileSystemObserver::Changes> ParseChanges(
    std::vector<mojom::FSPChangePtr> changes) {
  auto results = std::make_unique<ProvidedFileSystemObserver::Changes>();
  for (auto& change : changes) {
    results->push_back(ParseChange(std::move(change)));
  }
  return results;
}

std::optional<GURL> ToPNGDataURL(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    return std::nullopt;
  }
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(*image.bitmap(), false, &output);
  GURL url("data:image/png;base64," + base::Base64Encode(output));
  if (url.spec().size() > url::kMaxURLChars) {
    return std::nullopt;
  }
  return url;
}

}  // namespace

FileSystemProviderServiceAsh::FileSystemProviderServiceAsh() = default;
FileSystemProviderServiceAsh::~FileSystemProviderServiceAsh() = default;

void FileSystemProviderServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::FileSystemProviderService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FileSystemProviderServiceAsh::RegisterFileSystemProvider(
    mojo::PendingRemote<mojom::FileSystemProvider> provider) {
  remotes_.Add(mojo::Remote<mojom::FileSystemProvider>(std::move(provider)));
}

void FileSystemProviderServiceAsh::Mount(mojom::FileSystemMetadataPtr metadata,
                                         bool persistent,
                                         MountCallback callback) {
  MountWithProfile(std::move(metadata), persistent, std::move(callback),
                   ProfileManager::GetPrimaryUserProfile());
}

void FileSystemProviderServiceAsh::Unmount(
    mojom::FileSystemIdPtr file_system_id,
    UnmountCallback callback) {
  UnmountWithProfile(std::move(file_system_id), std::move(callback),
                     ProfileManager::GetPrimaryUserProfile());
}

void FileSystemProviderServiceAsh::GetAll(const std::string& provider,
                                          GetAllCallback callback) {
  GetAllWithProfile(provider, std::move(callback),
                    ProfileManager::GetPrimaryUserProfile());
}
void FileSystemProviderServiceAsh::Get(mojom::FileSystemIdPtr file_system_id,
                                       GetCallback callback) {
  GetWithProfile(std::move(file_system_id), std::move(callback),
                 ProfileManager::GetPrimaryUserProfile());
}
void FileSystemProviderServiceAsh::Notify(
    mojom::FileSystemIdPtr file_system_id,
    mojom::FSPWatcherPtr watcher,
    mojom::FSPChangeType type,
    std::vector<mojom::FSPChangePtr> changes,
    NotifyCallback callback) {
  NotifyWithProfile(std::move(file_system_id), std::move(watcher), type,
                    std::move(changes), std::move(callback),
                    ProfileManager::GetPrimaryUserProfile());
}

void FileSystemProviderServiceAsh::DeprecatedOperationFinished(
    mojom::FSPOperationResponse response,
    mojom::FileSystemIdPtr file_system_id,
    int64_t request_id,
    std::vector<base::Value> args,
    OperationFinishedCallback callback) {
  base::Value::List list;
  for (auto& value : args) {
    list.Append(std::move(value));
  }
  OperationFinished(response, std::move(file_system_id), request_id,
                    std::move(list), std::move(callback));
}

void FileSystemProviderServiceAsh::OperationFinished(
    mojom::FSPOperationResponse response,
    mojom::FileSystemIdPtr file_system_id,
    int64_t request_id,
    base::Value::List args,
    OperationFinishedCallback callback) {
  OperationFinishedWithProfile(response, std::move(file_system_id), request_id,
                               std::move(args), std::move(callback),
                               ProfileManager::GetPrimaryUserProfile());
}

void FileSystemProviderServiceAsh::OpenFileFinishedSuccessfully(
    mojom::FileSystemIdPtr file_system_id,
    int64_t request_id,
    base::Value::List args,
    OperationFinishedCallback callback) {
  OpenFileFinishedSuccessfullyWithProfile(
      std::move(file_system_id), request_id, std::move(args),
      std::move(callback), ProfileManager::GetPrimaryUserProfile());
}

void FileSystemProviderServiceAsh::MountFinished(
    const std::string& extension_id,
    int64_t request_id,
    base::Value::List args,
    MountFinishedCallback callback) {
  MountFinishedWithProfile(extension_id, request_id, std::move(args),
                           std::move(callback),
                           ProfileManager::GetPrimaryUserProfile());
}

void FileSystemProviderServiceAsh::ExtensionLoadedDeprecated(
    bool configurable,
    bool watchable,
    bool multiple_mounts,
    mojom::FileSystemSource source,
    const std::string& name,
    const std::string& id) {
  ExtensionLoaded(configurable, watchable, multiple_mounts, source, name, id,
                  /*icon16x16=*/gfx::ImageSkia(),
                  /*icon32x32=*/gfx::ImageSkia());
}

void FileSystemProviderServiceAsh::ExtensionLoaded(
    bool configurable,
    bool watchable,
    bool multiple_mounts,
    mojom::FileSystemSource source,
    const std::string& name,
    const std::string& id,
    const gfx::ImageSkia& icon16x16,
    const gfx::ImageSkia& icon32x32) {
  Service* const service =
      Service::Get(ProfileManager::GetPrimaryUserProfile());
  DCHECK(service);

  ProviderId provider_id = ProviderId::CreateFromExtensionId(id);
  extensions::FileSystemProviderSource extension_source;
  switch (source) {
    case crosapi::mojom::FileSystemSource::kFile:
      extension_source = extensions::FileSystemProviderSource::SOURCE_FILE;
      break;
    case crosapi::mojom::FileSystemSource::kNetwork:
      extension_source = extensions::FileSystemProviderSource::SOURCE_NETWORK;
      break;
    case crosapi::mojom::FileSystemSource::kDevice:
      extension_source = extensions::FileSystemProviderSource::SOURCE_DEVICE;
      break;
  }

  std::optional<IconSet> icon_set;
  std::optional<GURL> url_icon16x16 = ToPNGDataURL(icon16x16);
  std::optional<GURL> url_icon32x32 = ToPNGDataURL(icon32x32);
  if (url_icon16x16 && url_icon32x32) {
    icon_set = IconSet();
    icon_set->SetIcon(IconSet::IconSize::SIZE_16x16, *url_icon16x16);
    icon_set->SetIcon(IconSet::IconSize::SIZE_32x32, *url_icon32x32);
  }

  auto provider =
      std::make_unique<ash::file_system_provider::ExtensionProvider>(
          ProfileManager::GetPrimaryUserProfile(), std::move(provider_id),
          ash::file_system_provider::Capabilities{
              .configurable = configurable,
              .watchable = watchable,
              .multiple_mounts = multiple_mounts,
              .source = extension_source},
          name, icon_set);
  service->RegisterProvider(std::move(provider));
}

void FileSystemProviderServiceAsh::ExtensionUnloaded(const std::string& id,
                                                     bool due_to_shutdown) {
  Service* const service =
      Service::Get(ProfileManager::GetPrimaryUserProfile());
  DCHECK(service);
  ProviderId provider_id = ProviderId::CreateFromExtensionId(id);
  service->UnregisterProvider(
      provider_id,
      due_to_shutdown
          ? ash::file_system_provider::Service::UNMOUNT_REASON_SHUTDOWN
          : ash::file_system_provider::Service::UNMOUNT_REASON_USER);
}

void FileSystemProviderServiceAsh::MountWithProfile(
    mojom::FileSystemMetadataPtr metadata,
    bool persistent,
    MountCallback callback,
    Profile* profile) {
  Service* const service = Service::Get(profile);
  DCHECK(service);

  MountOptions options;
  options.file_system_id = metadata->file_system_id->id;
  options.display_name = metadata->display_name;
  options.writable = metadata->writable;
  options.opened_files_limit =
      base::saturated_cast<int>(metadata->opened_files_limit);
  options.supports_notify_tag = metadata->supports_notify;
  options.persistent = persistent;

  const base::File::Error result = service->MountFileSystem(
      ProviderId::CreateFromExtensionId(metadata->file_system_id->provider),
      options);
  RunErrorCallback(std::move(callback), result);
}

void FileSystemProviderServiceAsh::UnmountWithProfile(
    mojom::FileSystemIdPtr file_system_id,
    UnmountCallback callback,
    Profile* profile) {
  Service* const service = Service::Get(profile);
  const base::File::Error result = service->UnmountFileSystem(
      ProviderId::CreateFromExtensionId(file_system_id->provider),
      file_system_id->id, Service::UNMOUNT_REASON_USER);
  RunErrorCallback(std::move(callback), result);
}

void FileSystemProviderServiceAsh::GetAllWithProfile(
    const std::string& provider,
    GetAllCallback callback,
    Profile* profile) {
  Service* const service = Service::Get(profile);
  ProviderId provider_id = ProviderId::CreateFromExtensionId(provider);
  const std::vector<ProvidedFileSystemInfo> file_systems =
      service->GetProvidedFileSystemInfoList(provider_id);

  std::vector<crosapi::mojom::FileSystemInfoPtr> items;

  for (const auto& file_system_info : file_systems) {
    items.push_back(
        ConvertFileSystemToMojom(profile, file_system_info, provider));
  }

  std::move(callback).Run(std::move(items));
}

void FileSystemProviderServiceAsh::FileSystemProviderServiceAsh::GetWithProfile(
    mojom::FileSystemIdPtr file_system_id,
    GetCallback callback,
    Profile* profile) {
  Service* const service = Service::Get(profile);
  DCHECK(service);

  ProvidedFileSystemInterface* file_system = service->GetProvidedFileSystem(
      ProviderId::CreateFromExtensionId(file_system_id->provider),
      file_system_id->id);
  if (!file_system) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(ConvertFileSystemToMojom(
      profile, file_system->GetFileSystemInfo(), file_system_id->provider));
}

void FileSystemProviderServiceAsh::NotifyWithProfile(
    mojom::FileSystemIdPtr file_system_id,
    mojom::FSPWatcherPtr watcher,
    mojom::FSPChangeType type,
    std::vector<mojom::FSPChangePtr> changes,
    NotifyCallback callback,
    Profile* profile) {
  Service* const service = Service::Get(profile);
  DCHECK(service);

  ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(
          ProviderId::CreateFromExtensionId(file_system_id->provider),
          file_system_id->id);
  if (!file_system) {
    std::move(callback).Run(
        extensions::FileErrorToString(base::File::FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system->Notify(watcher->entry_path, watcher->recursive,
                      ParseChangeType(type), ParseChanges(std::move(changes)),
                      watcher->last_tag,
                      base::BindOnce(&RunErrorCallback, std::move(callback)));
}

void FileSystemProviderServiceAsh::OperationFinishedWithProfile(
    mojom::FSPOperationResponse response,
    mojom::FileSystemIdPtr file_system_id,
    int64_t request_id,
    base::Value::List args,
    OperationFinishedCallback callback,
    Profile* profile) {
  std::string error;
  switch (response) {
    case mojom::FSPOperationResponse::kUnknown:
      error = "unknown operation response";
      break;
    case mojom::FSPOperationResponse::kUnmountSuccess: {
      using extensions::api::file_system_provider_internal::
          UnmountRequestedSuccess::Params;
      std::optional<Params> params = Params::Create(std::move(args));
      if (!params) {
        error = kDeserializationError;
        break;
      }
      auto value = RequestValue::CreateForUnmountSuccess(std::move(*params));
      error = ForwardOperationResponse(std::move(file_system_id), request_id,
                                       value, /*has_more=*/false, profile);
      break;
    }
    case mojom::FSPOperationResponse::kGetEntryMetadataSuccess: {
      using extensions::api::file_system_provider_internal::
          GetMetadataRequestedSuccess::Params;
      std::optional<Params> params = Params::Create(std::move(args));
      if (!params) {
        error = kDeserializationError;
        break;
      }
      auto value =
          RequestValue::CreateForGetMetadataSuccess(std::move(*params));
      error = ForwardOperationResponse(std::move(file_system_id), request_id,
                                       value, /*has_more=*/false, profile);
      break;
    }
    case mojom::FSPOperationResponse::kGetActionsSuccess: {
      using extensions::api::file_system_provider_internal::
          GetActionsRequestedSuccess::Params;
      std::optional<Params> params = Params::Create(std::move(args));
      if (!params) {
        error = kDeserializationError;
        break;
      }
      auto value = RequestValue::CreateForGetActionsSuccess(std::move(*params));
      error = ForwardOperationResponse(std::move(file_system_id), request_id,
                                       value, /*has_more=*/false, profile);
      break;
    }
    case mojom::FSPOperationResponse::kReadDirectorySuccess: {
      using extensions::api::file_system_provider_internal::
          ReadDirectoryRequestedSuccess::Params;
      std::optional<Params> params = Params::Create(std::move(args));
      if (!params) {
        error = kDeserializationError;
        break;
      }
      bool has_more = params->has_more;
      auto value =
          RequestValue::CreateForReadDirectorySuccess(std::move(*params));
      error = ForwardOperationResponse(std::move(file_system_id), request_id,
                                       value, has_more, profile);
      break;
    }
    case mojom::FSPOperationResponse::kReadFileSuccess: {
      TRACE_EVENT0("file_system_provider", "ReadFileSuccessWithProfile");
      using extensions::api::file_system_provider_internal::
          ReadFileRequestedSuccess::Params;
      std::optional<Params> params = Params::Create(std::move(args));
      if (!params) {
        error = kDeserializationError;
        break;
      }
      bool has_more = params->has_more;
      auto value = RequestValue::CreateForReadFileSuccess(std::move(*params));
      error = ForwardOperationResponse(std::move(file_system_id), request_id,
                                       value, has_more, profile);
      break;
    }
    case mojom::FSPOperationResponse::kOpenFileSuccess: {
      TRACE_EVENT0("file_system_provider", "OpenFileSuccessWithProfile");
      using extensions::api::file_system_provider_internal::
          OpenFileRequestedSuccess::Params;
      std::optional<Params> params = Params::Create(std::move(args));
      if (!params) {
        error = kDeserializationError;
        break;
      }
      auto value = RequestValue::CreateForOpenFileSuccess(std::move(*params));
      error = ForwardOperationResponse(std::move(file_system_id), request_id,
                                       value, /*has_more=*/false, profile);
      break;
    }
    case mojom::FSPOperationResponse::kGenericSuccess: {
      using extensions::api::file_system_provider_internal::
          OperationRequestedSuccess::Params;
      std::optional<Params> params = Params::Create(std::move(args));
      if (!params) {
        error = kDeserializationError;
        break;
      }
      auto value = RequestValue::CreateForOperationSuccess(std::move(*params));
      error = ForwardOperationResponse(std::move(file_system_id), request_id,
                                       value, /*has_more=*/false, profile);
      break;
    }
    case mojom::FSPOperationResponse::kGenericFailure: {
      using extensions::api::file_system_provider_internal::
          OperationRequestedError::Params;
      std::optional<Params> params = Params::Create(std::move(args));
      if (!params) {
        error = kDeserializationError;
        break;
      }
      base::File::Error operation_error =
          extensions::ProviderErrorToFileError(params->error);
      auto value = RequestValue::CreateForOperationError(std::move(*params));
      error = ForwardOperationFailure(std::move(file_system_id), request_id,
                                      value, operation_error, profile);
      break;
    }
  }
  std::move(callback).Run(std::move(error));
}

void FileSystemProviderServiceAsh::OpenFileFinishedSuccessfullyWithProfile(
    mojom::FileSystemIdPtr file_system_id,
    int64_t request_id,
    base::Value::List args,
    OperationFinishedCallback callback,
    Profile* profile) {
  using extensions::api::file_system_provider_internal::
      OpenFileRequestedSuccess::Params;
  std::optional<Params> params = Params::Create(std::move(args));
  if (!params) {
    std::move(callback).Run(kDeserializationError);
  }
  auto value = RequestValue::CreateForOpenFileSuccess(std::move(*params));
  std::string error =
      ForwardOperationResponse(std::move(file_system_id), request_id, value,
                               /*has_more=*/false, profile);
  std::move(callback).Run(std::move(error));
}

void FileSystemProviderServiceAsh::MountFinishedWithProfile(
    const std::string& extension_id,
    int64_t request_id,
    base::Value::List args,
    MountFinishedCallback callback,
    Profile* profile) {
  auto manager = GetProviderRequestManager(profile, extension_id);
  if (!manager.has_value()) {
    std::move(callback).Run(manager.error());
    return;
  }

  using extensions::api::file_system_provider_internal::RespondToMountRequest::
      Params;
  std::optional<Params> params = Params::Create(std::move(args));
  if (!params) {
    std::move(callback).Run(kDeserializationError);
    return;
  }
  base::File::Error mount_error =
      extensions::ProviderErrorToFileError(params->error);
  base::File::Error result =
      mount_error == base::File::FILE_OK
          ? manager.value()->FulfillRequest(request_id,
                                            /*response=*/RequestValue(),
                                            /*has_more=*/false)
          : manager.value()->RejectRequest(
                request_id, /*response=*/RequestValue(), mount_error);

  std::string error_str;
  if (result != base::File::FILE_OK)
    error_str = extensions::FileErrorToString(result);
  std::move(callback).Run(error_str);
}

}  // namespace crosapi
