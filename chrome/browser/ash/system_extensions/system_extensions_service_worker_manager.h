// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SERVICE_WORKER_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SERVICE_WORKER_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

class Profile;

namespace ash {

class SystemExtensionsRegistry;

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

 private:
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
