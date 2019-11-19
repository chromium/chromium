// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/service.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provider_interface.h"
#include "chrome/browser/chromeos/file_system_provider/registry.h"
#include "chrome/browser/chromeos/file_system_provider/registry_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/chromeos/file_system_provider/throttled_file_system.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Maximum number of file systems to be mounted in the same time, per profile.
const size_t kMaxFileSystems = 16;

}  // namespace

Service::Service(Profile* profile,
                 extensions::ExtensionRegistry* extension_registry)
    : profile_(profile),
      extension_registry_(extension_registry),
      registry_(new Registry(profile)) {
  extension_registry_->AddObserver(this);
}

Service::~Service() {}

// static
Service* Service::Get(content::BrowserContext* context) {
  return ServiceFactory::Get(context);
}

void Service::Shutdown() {
  extension_registry_->RemoveObserver(this);

  // Provided file systems should be already unmounted because of receiving
  // OnExtensionUnload calls for each installed extension. However, for tests
  // we may still have mounted extensions.
  // TODO(mtomasz): Create a TestingService class and remove this code.
  auto it = file_system_map_.begin();
  while (it != file_system_map_.end()) {
    const std::string file_system_id =
        it->second->GetFileSystemInfo().file_system_id();
    const ProviderId provider_id =
        it->second->GetFileSystemInfo().provider_id();
    ++it;
    const base::File::Error unmount_result =
        UnmountFileSystem(provider_id, file_system_id, UNMOUNT_REASON_SHUTDOWN);
    DCHECK_EQ(base::File::FILE_OK, unmount_result);
  }

  DCHECK_EQ(0u, file_system_map_.size());
}

void Service::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void Service::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void Service::SetRegistryForTesting(
    std::unique_ptr<RegistryInterface> registry) {
  DCHECK(registry);
  registry_ = std::move(registry);
}

base::File::Error Service::MountFileSystem(const ProviderId& provider_id,
                                           const MountOptions& options) {
  return MountFileSystemInternal(provider_id, options, MOUNT_CONTEXT_USER);
}

base::File::Error Service::MountFileSystemInternal(
    const ProviderId& provider_id,
    const MountOptions& options,
    MountContext context) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ProviderInterface* const provider = GetProvider(provider_id);
  if (!provider) {
    for (auto& observer : observers_) {
      observer.OnProvidedFileSystemMount(
          ProvidedFileSystemInfo(), context,
          base::File::FILE_ERROR_INVALID_OPERATION);
    }
    return base::File::FILE_ERROR_INVALID_OPERATION;
  }

  // The mount point path and name are unique per system, since they are system
  // wide. This is necessary for copying between profiles.
  const base::FilePath& mount_path =
      util::GetMountPath(profile_, provider_id, options.file_system_id);
  const std::string mount_point_name = mount_path.BaseName().AsUTF8Unsafe();

  Capabilities capabilities = provider->GetCapabilities();
  // Store the file system descriptor. Use the mount point name as the file
  // system provider file system id.
  // Examples:
  //   file_system_id = hello_world
  //   mount_point_name =  b33f1337-hello_world-5aa5
  //   writable = false
  //   supports_notify_tag = false
  //   mount_path = /provided/b33f1337-hello_world-5aa5
  //   configurable = true
  //   watchable = true
  //   source = SOURCE_FILE
  ProvidedFileSystemInfo file_system_info(
      provider_id, options, mount_path, capabilities.configurable,
      capabilities.watchable, capabilities.source, provider->GetIconSet());

  // If already exists a file system provided by the same extension with this
  // id, then abort.
  if (GetProvidedFileSystem(provider_id, options.file_system_id)) {
    for (auto& observer : observers_) {
      observer.OnProvidedFileSystemMount(file_system_info, context,
                                         base::File::FILE_ERROR_EXISTS);
    }
    return base::File::FILE_ERROR_EXISTS;
  }

  // Restrict number of file systems to prevent system abusing.
  if (file_system_map_.size() + 1 > kMaxFileSystems) {
    for (auto& observer : observers_) {
      observer.OnProvidedFileSystemMount(
          ProvidedFileSystemInfo(), context,
          base::File::FILE_ERROR_TOO_MANY_OPENED);
    }
    return base::File::FILE_ERROR_TOO_MANY_OPENED;
  }

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  if (!mount_points->RegisterFileSystem(
          mount_point_name, storage::kFileSystemTypeProvided,
          storage::FileSystemMountOption(
              storage::FlushPolicy::FLUSH_ON_COMPLETION),
          mount_path)) {
    for (auto& observer : observers_) {
      observer.OnProvidedFileSystemMount(
          ProvidedFileSystemInfo(), context,
          base::File::FILE_ERROR_INVALID_OPERATION);
    }
    return base::File::FILE_ERROR_INVALID_OPERATION;
  }

  std::unique_ptr<ProvidedFileSystemInterface> file_system =
      provider->CreateProvidedFileSystem(profile_, file_system_info);
  DCHECK(file_system);
  ProvidedFileSystemInterface* file_system_ptr = file_system.get();
  file_system_map_[FileSystemKey(
      provider_id.ToString(), options.file_system_id)] = std::move(file_system);
  mount_point_name_to_key_map_[mount_point_name] =
      FileSystemKey(provider_id.ToString(), options.file_system_id);
  if (options.persistent) {
    const Watchers& watchers = file_system_info.watchable()
                                   ? *file_system_ptr->GetWatchers()
                                   : Watchers();
    registry_->RememberFileSystem(file_system_info, watchers);
  }

  for (auto& observer : observers_) {
    observer.OnProvidedFileSystemMount(file_system_info, context,
                                       base::File::FILE_OK);
  }

  return base::File::FILE_OK;
}

base::File::Error Service::UnmountFileSystem(const ProviderId& provider_id,
                                             const std::string& file_system_id,
                                             UnmountReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto file_system_it = file_system_map_.find(
      FileSystemKey(provider_id.ToString(), file_system_id));
  if (file_system_it == file_system_map_.end()) {
    const ProvidedFileSystemInfo empty_file_system_info;
    for (auto& observer : observers_) {
      observer.OnProvidedFileSystemUnmount(empty_file_system_info,
                                           base::File::FILE_ERROR_NOT_FOUND);
    }
    return base::File::FILE_ERROR_NOT_FOUND;
  }

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  const ProvidedFileSystemInfo& file_system_info =
      file_system_it->second->GetFileSystemInfo();

  const std::string mount_point_name =
      file_system_info.mount_path().BaseName().value();
  if (!mount_points->RevokeFileSystem(mount_point_name)) {
    for (auto& observer : observers_) {
      observer.OnProvidedFileSystemUnmount(
          file_system_info, base::File::FILE_ERROR_INVALID_OPERATION);
    }
    return base::File::FILE_ERROR_INVALID_OPERATION;
  }

  for (auto& observer : observers_)
    observer.OnProvidedFileSystemUnmount(file_system_info, base::File::FILE_OK);

  mount_point_name_to_key_map_.erase(mount_point_name);

  if (reason == UNMOUNT_REASON_USER) {
    registry_->ForgetFileSystem(file_system_info.provider_id(),
                                file_system_info.file_system_id());
  }

  file_system_map_.erase(file_system_it);

  return base::File::FILE_OK;
}

bool Service::RequestUnmount(const ProviderId& provider_id,
                             const std::string& file_system_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto file_system_it = file_system_map_.find(
      FileSystemKey(provider_id.ToString(), file_system_id));
  if (file_system_it == file_system_map_.end())
    return false;

  file_system_it->second->RequestUnmount(
      base::Bind(&Service::OnRequestUnmountStatus,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_system_it->second->GetFileSystemInfo()));
  return true;
}

bool Service::RequestMount(const ProviderId& provider_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ProviderInterface* const provider = GetProvider(provider_id);
  return provider->RequestMount(profile_);
}

std::vector<ProvidedFileSystemInfo> Service::GetProvidedFileSystemInfoList() {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<ProvidedFileSystemInfo> result;
  for (auto it = file_system_map_.begin(); it != file_system_map_.end(); ++it) {
    result.push_back(it->second->GetFileSystemInfo());
  }
  return result;
}

std::vector<ProvidedFileSystemInfo> Service::GetProvidedFileSystemInfoList(
    const ProviderId& provider_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::vector<ProvidedFileSystemInfo> full_list =
      GetProvidedFileSystemInfoList();
  std::vector<ProvidedFileSystemInfo> filtered_list;

  for (const auto& file_system : full_list) {
    if (file_system.provider_id() == provider_id) {
      filtered_list.push_back(file_system);
    }
  }

  return filtered_list;
}

ProvidedFileSystemInterface* Service::GetProvidedFileSystem(
    const ProviderId& provider_id,
    const std::string& file_system_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto file_system_it = file_system_map_.find(
      FileSystemKey(provider_id.ToString(), file_system_id));
  if (file_system_it == file_system_map_.end())
    return NULL;

  return file_system_it->second.get();
}

const Service::ProviderMap& Service::GetProviders() const {
  return provider_map_;
}

void Service::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension,
                                  extensions::UnloadedExtensionReason reason) {
  ProviderId provider_id = ProviderId::CreateFromExtensionId(extension->id());
  UnregisterProvider(
      provider_id,
      reason == extensions::UnloadedExtensionReason::PROFILE_SHUTDOWN
          ? UNMOUNT_REASON_SHUTDOWN
          : UNMOUNT_REASON_USER);
}

void Service::UnmountFileSystems(const ProviderId& provider_id,
                                 UnmountReason reason) {
  // Unmount all of the provided file systems associated with this provider.
  auto it = file_system_map_.begin();
  while (it != file_system_map_.end()) {
    const ProvidedFileSystemInfo& file_system_info =
        it->second->GetFileSystemInfo();
    // Advance the iterator beforehand, otherwise it will become invalidated
    // by the UnmountFileSystem() call.
    ++it;
    if (file_system_info.provider_id() == provider_id) {
      const base::File::Error unmount_result =
          UnmountFileSystem(file_system_info.provider_id(),
                            file_system_info.file_system_id(), reason);
      DCHECK_EQ(base::File::FILE_OK, unmount_result);
    }
  }
}

void Service::UnregisterProvider(const ProviderId& provider_id,
                                 UnmountReason reason) {
  UnmountFileSystems(provider_id, reason);
  provider_map_.erase(provider_id);
}

void Service::OnExtensionLoaded(content::BrowserContext* browser_context,
                                const extensions::Extension* extension) {
  // If the extension is a provider, then register it.
  std::unique_ptr<ProviderInterface> provider =
      ExtensionProvider::Create(extension_registry_, extension->id());
  if (provider)
    RegisterProvider(std::move(provider));
}

void Service::RestoreFileSystems(const ProviderId& provider_id) {
  std::unique_ptr<RegistryInterface::RestoredFileSystems>
      restored_file_systems = registry_->RestoreFileSystems(provider_id);

  for (const auto& restored_file_system : *restored_file_systems) {
    const base::File::Error result = MountFileSystemInternal(
        restored_file_system.provider_id, restored_file_system.options,
        MOUNT_CONTEXT_RESTORE);
    if (result != base::File::FILE_OK) {
      LOG(ERROR) << "Failed to restore a provided file system from "
                 << "registry: " << restored_file_system.provider_id.ToString()
                 << ", " << restored_file_system.options.file_system_id << ", "
                 << restored_file_system.options.display_name << ".";
      // Since remounting of the file system failed, then remove it from
      // preferences to avoid remounting it over and over again with a failure.
      registry_->ForgetFileSystem(restored_file_system.provider_id,
                                  restored_file_system.options.file_system_id);
      continue;
    }

    ProvidedFileSystemInterface* const file_system =
        GetProvidedFileSystem(restored_file_system.provider_id,
                              restored_file_system.options.file_system_id);
    DCHECK(file_system);

    if (file_system->GetFileSystemInfo().watchable()) {
      file_system->GetWatchers()->insert(restored_file_system.watchers.begin(),
                                         restored_file_system.watchers.end());
    }
  }
}

ProvidedFileSystemInterface* Service::GetProvidedFileSystem(
    const std::string& mount_point_name) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto mapping_it = mount_point_name_to_key_map_.find(mount_point_name);
  if (mapping_it == mount_point_name_to_key_map_.end())
    return NULL;

  const auto file_system_it = file_system_map_.find(mapping_it->second);
  if (file_system_it == file_system_map_.end())
    return NULL;

  return file_system_it->second.get();
}

void Service::OnRequestUnmountStatus(
    const ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  // Notify observers about failure in unmounting, since mount() will not be
  // called by the provided file system. In case of success mount() will be
  // invoked, and observers notified, so there is no need to call them now.
  if (error != base::File::FILE_OK) {
    for (auto& observer : observers_)
      observer.OnProvidedFileSystemUnmount(file_system_info, error);
  }
}

void Service::OnWatcherChanged(const ProvidedFileSystemInfo& file_system_info,
                               const Watcher& watcher,
                               storage::WatcherManager::ChangeType change_type,
                               const Changes& changes,
                               const base::Closure& callback) {
  callback.Run();
}

void Service::OnWatcherTagUpdated(
    const ProvidedFileSystemInfo& file_system_info,
    const Watcher& watcher) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  registry_->UpdateWatcherTag(file_system_info, watcher);
}

void Service::OnWatcherListChanged(
    const ProvidedFileSystemInfo& file_system_info,
    const Watchers& watchers) {
  registry_->RememberFileSystem(file_system_info, watchers);
}

void Service::RegisterProvider(std::unique_ptr<ProviderInterface> provider) {
  ProviderId provider_id = provider->GetId();
  provider_map_[provider_id] = std::move(provider);
  RestoreFileSystems(provider_id);
}

ProviderInterface* Service::GetProvider(const ProviderId& provider_id) {
  DCHECK_NE(ProviderId::INVALID, provider_id.GetType());
  auto it = provider_map_.find(provider_id);
  if (it == provider_map_.end())
    return nullptr;

  return it->second.get();
}

}  // namespace file_system_provider
}  // namespace chromeos
