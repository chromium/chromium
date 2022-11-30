// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SERVICE_WORKER_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SERVICE_WORKER_MANAGER_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_status_or.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

class Profile;

namespace ash {

class SystemExtensionsRegistry;

struct SystemExtensionsServiceWorkerInfo {
  SystemExtensionId system_extension_id;
  int64_t service_worker_version_id;
  int service_worker_process_id;

  // Operator so that the struct can be used in STL containers.
  constexpr bool operator<(
      const SystemExtensionsServiceWorkerInfo& other) const {
    return std::tie(system_extension_id, service_worker_version_id,
                    service_worker_process_id) <
           std::tie(other.system_extension_id, other.service_worker_version_id,
                    other.service_worker_process_id);
  }
};

using StatusOrSystemExtensionsServiceWorkerInfo =
    SystemExtensionsStatusOr<blink::ServiceWorkerStatusCode,
                             SystemExtensionsServiceWorkerInfo>;

// Class to register, unregister, and start service workers.
class SystemExtensionsServiceWorkerManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnRegisterServiceWorker(
        const SystemExtensionId& system_extension_id,
        blink::ServiceWorkerStatusCode status_code) {}
    virtual void OnUnregisterServiceWorker(
        const SystemExtensionId& system_extension_id,
        bool succeeded) {}
  };

  SystemExtensionsServiceWorkerManager(Profile* profile,
                                       SystemExtensionsRegistry& registry);
  SystemExtensionsServiceWorkerManager(
      const SystemExtensionsServiceWorkerManager&) = delete;
  SystemExtensionsServiceWorkerManager& operator=(
      const SystemExtensionsServiceWorkerManager&) = delete;
  ~SystemExtensionsServiceWorkerManager();

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // Registers a Service Worker for a System Extension with
  // `system_extension_id`.
  void RegisterServiceWorker(const SystemExtensionId& system_extension_id);

  // Unregisters a Service Worker for a System Extension with
  // `system_extension_id`.
  void UnregisterServiceWorker(const SystemExtensionId& system_extension_id);

  // Starts the service worker for System Extension with `system_extension_id`.
  using StartServiceWorkerCallback =
      base::OnceCallback<void(StatusOrSystemExtensionsServiceWorkerInfo)>;
  void StartServiceWorker(const SystemExtensionId& system_extension_id,
                          StartServiceWorkerCallback callback);

 private:
  void OnStartServiceWorkerSuccess(const SystemExtensionId& system_extension_id,
                                   StartServiceWorkerCallback callback,
                                   int64_t service_worker_version_id,
                                   int service_worker_process_id,
                                   int service_worker_thread_id);
  void OnStartServiceWorkerFailure(
      const SystemExtensionId& system_extension_id,
      StartServiceWorkerCallback callback,
      blink::ServiceWorkerStatusCode service_worker_status_code);

  void NotifyServiceWorkerRegistered(
      const SystemExtensionId& system_extension_id,
      blink::ServiceWorkerStatusCode status_code);
  void NotifyServiceWorkerUnregistered(
      const SystemExtensionId& system_extension_id,
      bool succeeded);

  // Safe because this class is owned by SystemExtensionsProvider which is owned
  // by the profile.
  raw_ptr<Profile> profile_;

  // Safe to hold references because the parent class,
  // SystemExtensionsProvider, ensures this class is constructed after and
  // destroyed before the classes below.
  const raw_ref<SystemExtensionsRegistry> registry_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<SystemExtensionsServiceWorkerManager> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SERVICE_WORKER_MANAGER_H_
