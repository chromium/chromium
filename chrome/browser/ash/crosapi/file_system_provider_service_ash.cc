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

std::string ForwardOperationResponseImpl(mojom::FileSystemIdPtr file_system_id,
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

std::string ForwardOperationFailureImpl(mojom::FileSystemIdPtr file_system_id,
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
  std::optional<std::vector<uint8_t>> output =
      gfx::PNGCodec::EncodeBGRASkBitmap(*image.bitmap(), false);
  GURL url("data:image/png;base64," +
           base::Base64Encode(output.value_or(std::vector<uint8_t>())));
  if (url.spec().size() > url::kMaxURLChars) {
    return std::nullopt;
  }
  return url;
}

}  // namespace

FileSystemProviderServiceAsh::FileSystemProviderServiceAsh() = default;
FileSystemProviderServiceAsh::~FileSystemProviderServiceAsh() = default;

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

std::string FileSystemProviderServiceAsh::ForwardOperationResponse(
    mojom::FileSystemIdPtr file_system_id,
    int64_t request_id,
    const RequestValue& value,
    bool has_more,
    Profile* profile) {
  return ForwardOperationResponseImpl(std::move(file_system_id), request_id,
                                      value, has_more, profile);
}

std::string FileSystemProviderServiceAsh::ForwardOperationFailure(
    mojom::FileSystemIdPtr file_system_id,
    int64_t request_id,
    const RequestValue& value,
    base::File::Error error,
    Profile* profile) {
  return ForwardOperationFailureImpl(std::move(file_system_id), request_id,
                                     value, error, profile);
}

}  // namespace crosapi
