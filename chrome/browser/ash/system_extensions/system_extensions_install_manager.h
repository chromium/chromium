// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_

#include <map>

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_status.h"
#include "chrome/browser/ash/system_extensions/system_extensions_sandboxed_unpacker.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

class Profile;

namespace ash {

class SystemExtensionsPersistentStorage;
class SystemExtensionsRegistry;
class SystemExtensionsRegistryManager;
class SystemExtensionsServiceWorkerManager;

class SystemExtensionsInstallManager {
 public:
  // Observer for installation and uninstallation steps events. This should be
  // used for classes that need to perform an action in response to an
  // installation step.
  // TODO(b/241308071): Once it's implemented, suggest using
  // SystemExtensionsRegistry::Observer for clients that only care about when
  // a System Extension is installed or uninstalled.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSystemExtensionAssetsDeleted(
        const SystemExtensionId& system_extension_id,
        bool succeeded) {}
  };

  SystemExtensionsInstallManager(
      Profile* profile,
      SystemExtensionsRegistryManager& registry_manager,
      SystemExtensionsRegistry& registry,
      SystemExtensionsServiceWorkerManager& service_worker_manager,
      SystemExtensionsPersistentStorage& persistent_storage);
  SystemExtensionsInstallManager(const SystemExtensionsInstallManager&) =
      delete;
  SystemExtensionsInstallManager& operator=(
      const SystemExtensionsInstallManager&) = delete;
  ~SystemExtensionsInstallManager();

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  using OnceInstallCallback =
      base::OnceCallback<void(InstallStatusOrSystemExtensionId)>;
  void InstallUnpackedExtensionFromDir(
      const base::FilePath& unpacked_system_extension_dir,
      OnceInstallCallback final_callback);

  // Event that signals when a System Extension is installed from the command
  // line. Signals even if the System Extension failed to be installed, but
  // doesn't if there were no command line arguments to install.
  const base::OneShotEvent& on_command_line_install_finished() {
    return on_command_line_install_finished_;
  }

  // Event that signals when all System Extensions that were persisted in a
  // previous session are registered.
  const base::OneShotEvent& on_register_previously_persisted_finished() {
    return on_register_previously_persisted_finished_;
  }

  // Uninstallation always succeeds.
  //
  // Currently only two operations can fail, unregistering the Service Worker,
  // and deleting the System Extension's assets. Regardless of these operations
  // failing, the System Extension will be considered uninstalled. Both of these
  // failure cases will be handled by a garbage collector outside of this class.
  // TODO(b/241198799): Change this comment once the garbage collector is
  // implemented.
  void Uninstall(const SystemExtensionId& system_extension_id);

 private:
  // Helper class to run blocking IO operations on a separate thread.
  class IOHelper {
   public:
    bool CopyExtensionAssets(const base::FilePath& unpacked_extension_dir,
                             const base::FilePath& dest_dir,
                             const base::FilePath& system_extensions_dir);
    bool RemoveExtensionAssets(const base::FilePath& system_extension_dir);
  };

  void RegisterPreviouslyPersistedSystemExtensions();
  void InstallFromCommandLineIfNecessary();
  void OnInstallFromCommandLineFinished(
      InstallStatusOrSystemExtensionId result);

  void StartInstallation(OnceInstallCallback final_callback,
                         const base::FilePath& unpacked_system_extension_dir);
  void OnGetSystemExtensionFromDir(
      OnceInstallCallback final_callback,
      const base::FilePath& unpacked_system_extension_dir,
      InstallStatusOrSystemExtension result);
  void OnAssetsCopiedToProfileDir(OnceInstallCallback final_callback,
                                  SystemExtension system_extension,
                                  bool did_succeed);
  void RegisterSystemExtension(SystemExtension system_extension);
  void RegisterServiceWorker(const SystemExtensionId& id);
  void DispatchWindowManagerStartEvent(const SystemExtensionId& id,
                                       int64_t version_id,
                                       int process_id,
                                       int thread_id);

  void NotifyServiceWorkerRegistered(
      const SystemExtensionId& id,
      blink::ServiceWorkerStatusCode status_code);
  void NotifyServiceWorkerUnregistered(const SystemExtensionId& id,
                                       bool succeeded);
  void NotifyAssetsRemoved(const SystemExtensionId&, bool succeeded);

  // Safe because this class is owned by SystemExtensionsProvider which is owned
  // by the profile.
  raw_ptr<Profile> profile_;

  // Safe to hold references because the parent class,
  // SystemExtensionsProvider, ensures this class is constructed after and
  // destroyed before the classes below.
  const raw_ref<SystemExtensionsServiceWorkerManager> service_worker_manager_;
  const raw_ref<SystemExtensionsRegistryManager> registry_manager_;
  const raw_ref<SystemExtensionsRegistry> registry_;
  const raw_ref<SystemExtensionsPersistentStorage> persistent_storage_;

  std::map<SystemExtensionId, SystemExtension> system_extensions_;

  base::OneShotEvent on_command_line_install_finished_;
  base::OneShotEvent on_register_previously_persisted_finished_;

  SystemExtensionsSandboxedUnpacker sandboxed_unpacker_;

  base::ObserverList<Observer> observers_;

  base::SequenceBound<IOHelper> io_helper_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_VISIBLE})};

  base::WeakPtrFactory<SystemExtensionsInstallManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_
