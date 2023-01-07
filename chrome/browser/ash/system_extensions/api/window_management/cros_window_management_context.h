// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_

#include <set>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_service_worker_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

class Profile;

namespace ash {

class WindowManagementImpl;

// Class in charge of managing CrosWindowManagement instances and dispatching
// events to them.
//
// Owns receivers for blink::mojom::CrosWindowManagementFactory and associated
// receivers and implementations for blink::mojom::CrosWindowManagement.
class CrosWindowManagementContext
    : public KeyedService,
      public ui::EventHandler,
      public SystemExtensionsServiceWorkerManager::Observer,
      public blink::mojom::CrosWindowManagementFactory,
      public apps::InstanceRegistry::Observer {
 public:
  // Returns the event dispatcher associated with `profile`. Should only be
  // called if System Extensions is enabled for the profile i.e. if
  // IsSystemExtensionsEnabled() returns true.
  static CrosWindowManagementContext& Get(Profile* profile);

  // Binds |pending_receiver| to |this| which implements
  // CrosWindowManagementFactory. |pending_receiver| is added to a
  // mojo::ReceiverSet<> so that it gets deleted when the connection is
  // broken.
  static void BindFactory(
      Profile* profile,
      const content::ServiceWorkerVersionBaseInfo& info,
      mojo::PendingReceiver<blink::mojom::CrosWindowManagementFactory>
          pending_receiver);

  explicit CrosWindowManagementContext(Profile* profile);
  CrosWindowManagementContext(const CrosWindowManagementContext&) = delete;
  CrosWindowManagementContext& operator=(const CrosWindowManagementContext&) =
      delete;
  ~CrosWindowManagementContext() override;

  // ui::EventHandler
  void OnKeyEvent(ui::KeyEvent* event) override;

  // apps::InstanceRegistry::Observer
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override;

  // SystemExtensionsServiceWorkerManager::Observer
  void OnRegisterServiceWorker(
      const SystemExtensionId& system_extension_id,
      blink::ServiceWorkerStatusCode status_code) override;

  // blink::mojom::CrosWindowManagementFactory
  void Create(
      mojo::PendingAssociatedReceiver<blink::mojom::CrosWindowManagement>
          pending_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::CrosWindowManagementObserver>
          observer_remote) override;

 private:
  // Starts a Service Worker and gets the CrosWindowManagement for it.
  void GetCrosWindowManagement(
      const SystemExtensionId& system_extension_id,
      base::OnceCallback<void(WindowManagementImpl&)> callback);
  // Starts the Service Worker for all Window Management System Extensions
  // and runs `callback` with the CrosWindowManagement corresponding to the
  // Service Workers.
  void GetCrosWindowManagementInstances(
      base::RepeatingCallback<void(WindowManagementImpl&)> callback);

  void OnServiceWorkerStarted(
      const SystemExtensionId& system_extension_id,
      StatusOrSystemExtensionsServiceWorkerInfo status_or_info);

  void RunPendingTasks(const SystemExtensionId& system_extension_id,
                       WindowManagementImpl& window_management_impl);

  void OnCrosWindowManagementDisconnect();

  // This class is a BrowserContextKeyedService, so it's owned by Profile.
  const raw_ref<Profile> profile_;

  // Safe because this KeyedService is marked as depending on the
  // SystemExtensionsProvider keyed service which owns the classes below.
  const raw_ref<SystemExtensionsRegistry> system_extensions_registry_;
  const raw_ref<SystemExtensionsServiceWorkerManager>
      system_extensions_service_worker_manager_;

  base::ScopedObservation<SystemExtensionsServiceWorkerManager,
                          SystemExtensionsServiceWorkerManager::Observer>
      service_worker_manager_observation_{this};

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};

  mojo::ReceiverSet<blink::mojom::CrosWindowManagementFactory,
                    content::ServiceWorkerVersionBaseInfo>
      factory_receivers_;

  // Holds WindowManagementImpl instances and their receivers. They are
  // associated to factory instances in CrosWindowManagementContext and will be
  // destroyed whenever the corresponding factory gets destroyed.
  mojo::UniqueAssociatedReceiverSet<blink::mojom::CrosWindowManagement,
                                    SystemExtensionsServiceWorkerInfo>
      cros_window_management_instances_;

  // Maps worker info (including `SystemExtensionId`) to `WindowManagementImpl`.
  // `WindowManagementImpl` instances are owned by
  // `cros_window_management_instances_`.
  std::map<SystemExtensionsServiceWorkerInfo, WindowManagementImpl*>
      service_worker_info_to_impl_map_;

  using GetCrosWindowManagementCallback =
      base::OnceCallback<void(WindowManagementImpl&)>;
  using GetCrosWindowManagementCallbacks =
      std::vector<GetCrosWindowManagementCallback>;

  // Map of pending GetCrosWindowManagement callbacks for the System
  // Extension with `SystemExtensionId`.
  std::map<SystemExtensionId, GetCrosWindowManagementCallbacks>
      id_to_pending_callbacks_;

  base::WeakPtrFactory<CrosWindowManagementContext> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_
