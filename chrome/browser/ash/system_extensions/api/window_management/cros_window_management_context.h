// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_

#include <set>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom.h"

class Profile;

namespace ash {

// Class in charge of managing CrosWindowManagement instances and dispatching
// events to them.
//
// Owns receivers for blink::mojom::CrosWindowManagementFactory and associated
// receivers and implementations for blink::mojom::CrosWindowManagement.
class CrosWindowManagementContext
    : public KeyedService,
      public blink::mojom::CrosWindowManagementFactory {
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

  // blink::mojom::CrosWindowManagementFactory
  void Create(
      mojo::PendingAssociatedReceiver<blink::mojom::CrosWindowManagement>
          pending_receiver,
      mojo::PendingAssociatedRemote<
          blink::mojom::CrosWindowManagementStartObserver> observer_remote)
      override;

 private:
  // This class is a BrowserContextKeyedService, so it's owned by Profile.
  const raw_ref<Profile> profile_;

  // Safe because this KeyedService is marked as depending on the
  // SystemExtensionsProvider keyed service which owns
  // SystemExtensionsInstallManager.
  const raw_ref<SystemExtensionsInstallManager> install_manager_;

  mojo::ReceiverSet<blink::mojom::CrosWindowManagementFactory,
                    content::ServiceWorkerVersionBaseInfo>
      factory_receivers_;

  // Holds WindowManagementImpl instances. These receivers are associated to
  // factory instances in factory_receivers_ and will be destroyed whenever
  // the corresponding factory in factory_receivers_ gets destroyed.
  mojo::UniqueAssociatedReceiverSet<blink::mojom::CrosWindowManagement>
      cros_window_management_instances_;

  // Stores whether or not we've dispatched the 'start' event for the extension.
  std::set<SystemExtensionId> start_dispatched_for_extension_;

  base::WeakPtrFactory<CrosWindowManagementContext> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_
