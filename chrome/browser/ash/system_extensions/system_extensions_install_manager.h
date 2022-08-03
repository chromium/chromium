// Copyright 2021 The Chromium Authors. All rights reserved.
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

class SystemExtensionsRegistry;
class SystemExtensionsRegistryManager;

class SystemExtensionsInstallManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnServiceWorkerRegistered(
        const SystemExtensionId& system_extension_id,
        blink::ServiceWorkerStatusCode status_code) {}
  };

  SystemExtensionsInstallManager(
      Profile* profile,
      SystemExtensionsRegistryManager& registry_manager,
      SystemExtensionsRegistry& registry);
  SystemExtensionsInstallManager(const SystemExtensionsInstallManager&) =
      delete;
  SystemExtensionsInstallManager& operator=(
      const SystemExtensionsInstallManager&) = delete;
  ~SystemExtensionsInstallManager();

  using OnceInstallCallback =
      base::OnceCallback<void(InstallStatusOrSystemExtensionId)>;
  void InstallUnpackedExtensionFromDir(
      const base::FilePath& unpacked_system_extension_dir,
      OnceInstallCallback final_callback);

  const base::OneShotEvent& on_command_line_install_finished() {
    return on_command_line_install_finished_;
  }

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  // Helper class to run blocking IO operations on a separate thread.
  class IOHelper {
   public:
    bool CopyExtensionAssets(const base::FilePath& unpacked_extension_dir,
                             const base::FilePath& dest_dir,
                             const base::FilePath& system_extensions_dir);
  };

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
  void RegisterServiceWorker(const SystemExtensionId& id);
  void OnRegisterServiceWorker(const SystemExtensionId& id,
                               blink::ServiceWorkerStatusCode status_code);
  void DispatchWindowManagerStartEvent(const SystemExtensionId& id,
                                       int64_t version_id,
                                       int process_id,
                                       int thread_id);

  // Safe because this class is owned by SystemExtensionsProvider which is owned
  // by the profile.
  raw_ptr<Profile> profile_;

  // Safe to hold references because the parent class,
  // SystemExtensionsProvider, ensures this class is constructed after and
  // destroyed before the classes below.
  const raw_ref<SystemExtensionsRegistryManager> registry_manager_;
  const raw_ref<SystemExtensionsRegistry> registry_;

  std::map<SystemExtensionId, SystemExtension> system_extensions_;

  base::OneShotEvent on_command_line_install_finished_;

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
