// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/extension_provider.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_observer.h"
#include "chrome/browser/chromeos/file_system_provider/provider_interface.h"
#include "chrome/browser/chromeos/file_system_provider/watcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "storage/browser/file_system/watcher_manager.h"

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInfo;
class ProvidedFileSystemInterface;
class RegistryInterface;
struct MountOptions;

// Registers preferences to remember registered file systems between reboots.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Manages and registers the file system provider service. Maintains provided
// file systems.
class Service : public KeyedService,
                public extensions::ExtensionRegistryObserver,
                public ProvidedFileSystemObserver {
 public:
  typedef std::map<ProviderId, std::unique_ptr<ProviderInterface>> ProviderMap;

  // Reason for unmounting. In case of UNMOUNT_REASON_SHUTDOWN, the file system
  // will be remounted automatically after a reboot. In case of
  // UNMOUNT_REASON_USER it will be permanently unmounted.
  enum UnmountReason { UNMOUNT_REASON_USER, UNMOUNT_REASON_SHUTDOWN };

  Service(Profile* profile, extensions::ExtensionRegistry* extension_registry);
  ~Service() override;

  // Gets the singleton instance for the |context|.
  static Service* Get(content::BrowserContext* context);

  // KeyedService:
  void Shutdown() override;

  // Sets a custom Registry implementation. Used by unit tests.
  void SetRegistryForTesting(std::unique_ptr<RegistryInterface> registry);

  // Mounts a file system provided by a provider with the |provider_id|. If
  // |writable| is set to true, then the file system is mounted in a R/W mode.
  // Otherwise, only read-only operations are supported. If change notification
  // tags are supported, then |supports_notify_tag| must be true. Note, that
  // it is required in order to enable the internal cache. For success, returns
  // base::File::FILE_OK, otherwise an error code.
  base::File::Error MountFileSystem(const ProviderId& provider_id,
                                    const MountOptions& options);

  // Unmounts a file system with the specified |file_system_id| for the
  // |provider_id|. For success returns base::File::FILE_OK, otherwise an error
  // code.
  base::File::Error UnmountFileSystem(const ProviderId& provider_id,
                                      const std::string& file_system_id,
                                      UnmountReason reason);

  // Requests unmounting of the file system. Returns false if the request could
  // not been created, true otherwise.
  bool RequestUnmount(const ProviderId& provider_id,
                      const std::string& file_system_id);

  // Requests mounting a new file system by the providing extension with
  // |provider_id|. Returns false if the request could not been created, true
  // otherwise.
  bool RequestMount(const ProviderId& provider_id);

  // Returns a list of information of all currently provided file systems. All
  // items are copied.
  std::vector<ProvidedFileSystemInfo> GetProvidedFileSystemInfoList();

  // Returns a list of information of the currently provided file systems for
  // |provider_id|. All items are copied.
  std::vector<ProvidedFileSystemInfo> GetProvidedFileSystemInfoList(
      const ProviderId& provider_id);

  // Returns an immutable map of all registered providers.
  const ProviderMap& GetProviders() const;

  // Returns a provided file system with |file_system_id|, handled by
  // the extension with |provider_id|. If not found, then returns NULL.
  ProvidedFileSystemInterface* GetProvidedFileSystem(
      const ProviderId& provider_id,
      const std::string& file_system_id);

  // Returns a provided file system attached to the the passed
  // |mount_point_name|. If not found, then returns NULL.
  ProvidedFileSystemInterface* GetProvidedFileSystem(
      const std::string& mount_point_name);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;

  // ProvidedFileSystemInterface::Observer overrides.
  void OnWatcherChanged(const ProvidedFileSystemInfo& file_system_info,
                        const Watcher& watcher,
                        storage::WatcherManager::ChangeType change_type,
                        const ProvidedFileSystemObserver::Changes& changes,
                        const base::Closure& callback) override;
  void OnWatcherTagUpdated(const ProvidedFileSystemInfo& file_system_info,
                           const Watcher& watcher) override;
  void OnWatcherListChanged(const ProvidedFileSystemInfo& file_system_info,
                            const Watchers& watchers) override;

  // Registers a provider. Restores all remembered mounts.
  void RegisterProvider(std::unique_ptr<ProviderInterface> provider);

  // Unregisters a provider. Unmounts all currently mounted file systems.
  // If the reason is UNMOUNT_REASON_USER then they will not be automatically
  // restored on the next registration.
  void UnregisterProvider(const ProviderId& provider_id, UnmountReason reason);

 private:
  FRIEND_TEST_ALL_PREFIXES(FileSystemProviderServiceTest, RememberFileSystem);

  // Key is a pair of an extension id and file system id, which makes it
  // unique among the entire service instance.
  using FileSystemKey = std::pair<std::string, std::string>;

  // Mounts the file system in the specified context. See MountFileSystem() for
  // more information.
  base::File::Error MountFileSystemInternal(const ProviderId& provider_id,
                                            const MountOptions& options,
                                            MountContext context);

  // Called when the providing extension accepts or refuses a unmount request.
  // If |error| is equal to FILE_OK, then the request is accepted.
  void OnRequestUnmountStatus(const ProvidedFileSystemInfo& file_system_info,
                              base::File::Error error);

  // Remembers the file system in preferences, in order to remount after a
  // reboot.
  void RememberFileSystem(const ProvidedFileSystemInfo& file_system_info,
                          const Watchers& watchers);

  // Removes the file system from preferences, so it is not remounted anymore
  // after a reboot.
  void ForgetFileSystem(const ProviderId& provider_id,
                        const std::string& file_system_id);

  // Restores from preferences file systems mounted previously by the
  // |provider_id| provided file system.
  void RestoreFileSystems(const ProviderId& provider_id);

  // Unmounts all currently mounted file systems for this provider. If
  // reason is UNMOUNT_REASON_USER then the file systems will not be remembered
  // for automagical remount in the future.
  void UnmountFileSystems(const ProviderId& provider_id, UnmountReason reason);

  // Returns a file system provider for the passed |provider_id|. If not found
  // then returns nullptr.
  ProviderInterface* GetProvider(const ProviderId& provider_id);

  Profile* profile_;
  extensions::ExtensionRegistry* extension_registry_;  // Not owned.
  base::ObserverList<Observer>::Unchecked observers_;
  std::map<FileSystemKey, std::unique_ptr<ProvidedFileSystemInterface>>
      file_system_map_;
  std::map<std::string, FileSystemKey> mount_point_name_to_key_map_;
  std::unique_ptr<RegistryInterface> registry_;
  base::ThreadChecker thread_checker_;
  ProviderMap provider_map_;

  base::WeakPtrFactory<Service> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
