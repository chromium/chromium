// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ADBD_ARC_ADBD_MONITOR_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_ADBD_ARC_ADBD_MONITOR_BRIDGE_H_

#include "ash/components/arc/mojom/adbd.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcAdbdMonitorBridge
    : public KeyedService,
      public ConnectionObserver<mojom::AdbdMonitorInstance>,
      public mojom::AdbdMonitorHost {
 public:
  // Returns singleton instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcAdbdMonitorBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcAdbdMonitorBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // The constructor will register an Observer with ArcBridgeService.
  ArcAdbdMonitorBridge(content::BrowserContext* context,
                       ArcBridgeService* bridge_service);
  ~ArcAdbdMonitorBridge() override;
  ArcAdbdMonitorBridge(const ArcAdbdMonitorBridge&) = delete;
  ArcAdbdMonitorBridge& operator=(const ArcAdbdMonitorBridge&) = delete;

  // mojom::AdbdMonitorHost overrides:
  void AdbdStarted() override;
  void AdbdStopped() override;

  // ConnectionObserver<mojom::AdbdMonitorInstance> overrides:
  void OnConnectionReady() override;

  // Enables adb-over-usb for testing.
  void EnableAdbOverUsbForTesting();

  // Mostly the same as |AdbdStarted| / |AdbdStopped|, but takes a callback for
  // testing.
  void OnAdbdStartedForTesting(chromeos::VoidDBusMethodCallback callback);
  void OnAdbdStoppedForTesting(chromeos::VoidDBusMethodCallback callback);

  static void EnsureFactoryBuilt();

 private:
  void StartArcVmAdbd(chromeos::VoidDBusMethodCallback callback);
  void StopArcVmAdbd(chromeos::VoidDBusMethodCallback callback);
  void StartArcVmAdbdInternal(chromeos::VoidDBusMethodCallback,
                              bool adb_over_usb_enabled);
  void StopArcVmAdbdInternal(chromeos::VoidDBusMethodCallback,
                             bool adb_over_usb_enabled);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // For callbacks.
  base::WeakPtrFactory<ArcAdbdMonitorBridge> weak_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ADBD_ARC_ADBD_MONITOR_BRIDGE_H_
