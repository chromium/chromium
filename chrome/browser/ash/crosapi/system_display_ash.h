// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SYSTEM_DISPLAY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SYSTEM_DISPLAY_ASH_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/display/display_observer.h"

namespace crosapi {

// The ash-chrome implementation of the SystemDisplay crosapi interface.
// This class must only be used from the main thread.
// Display change is triggered by 2 sources:
// * "Source 1": display::Screen using display::DisplayObserver.
// * "Source 2": crosapi::mojom::CrosDisplayConfigController using
//   crosapi::mojom::CrosDisplayConfigObserver.
// To set up both sources, this class duplicates code from DisplayInfoProvider
// and DisplayInfoProviderChromeOS. This is necessary because the activation of
// these sources are managed differently: DisplayInfoProvider's sources are
// controlled by SystemInfoEventRouter, whereas SystemDisplayAsh's sources are
// controlled by |observers_| change.
class SystemDisplayAsh : public mojom::SystemDisplay,
                         public display::DisplayObserver,
                         public crosapi::mojom::CrosDisplayConfigObserver {
 public:
  // This type was generated from IDL.
  using DisplayUnitInfo = extensions::api::system_display::DisplayUnitInfo;

  SystemDisplayAsh();
  SystemDisplayAsh(const SystemDisplayAsh&) = delete;
  SystemDisplayAsh& operator=(const SystemDisplayAsh&) = delete;
  ~SystemDisplayAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::SystemDisplay> receiver);

  // crosapi::mojom::SystemDisplay:
  void GetDisplayUnitInfoList(bool single_unified,
                              GetDisplayUnitInfoListCallback callback) override;
  void AddDisplayChangeObserver(
      mojo::PendingRemote<mojom::DisplayChangeObserver> observer) override;

 private:
  // Receiver for extensions::DisplayInfoProvider::GetAllDisplaysInfo().
  void OnDisplayInfoResult(GetDisplayUnitInfoListCallback callback,
                           std::vector<DisplayUnitInfo> src_info_list);

  // Called when an observer added by AddDisplayChangeObserver() disconnects.
  void OnDisplayChangeObserverDisconnect(mojo::RemoteSetElementId /*id*/);

  // Dispatches display change events to observers (for all sources).
  void DispatchCrosapiDisplayChangeObservers();

  // display::DisplayObserver (for Source 1):
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t /*metrics*/) override;

  // crosapi::mojom::CrosDisplayConfigObserver (for Source 2):
  void OnDisplayConfigChanged() override;

  // Starts listening to display change event sources. No-op if already started.
  void StartDisplayChangedEventSources();

  // Stops listening to display change event sources. No-op if already stopped.
  void StopDisplayChangedEventSources();

  // Support any number of connections.
  mojo::ReceiverSet<mojom::SystemDisplay> receivers_;

  // Support any number of observers.
  mojo::RemoteSet<mojom::DisplayChangeObserver> observers_;

  // Source 1 status and objects.
  absl::optional<display::ScopedOptionalDisplayObserver> display_observer_;

  // Source 2 status and objects.
  bool is_observing_cros_display_config_ = false;
  mojo::Remote<crosapi::mojom::CrosDisplayConfigController>
      cros_display_config_;
  mojo::AssociatedReceiver<crosapi::mojom::CrosDisplayConfigObserver>
      cros_display_config_observer_receiver_{this};

  base::WeakPtrFactory<SystemDisplayAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SYSTEM_DISPLAY_ASH_H_
