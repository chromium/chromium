// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_ARC_WIFI_HOST_IMPL_H_
#define ASH_COMPONENTS_ARC_NET_ARC_WIFI_HOST_IMPL_H_

#include "ash/components/arc/mojom/arc_wifi.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/files/scoped_file.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Private implementation of ArcWifiHost.
class ArcWifiHostImpl : public KeyedService,
                        public ConnectionObserver<mojom::ArcWifiInstance>,
                        public ash::NetworkStateHandlerObserver,
                        public mojom::ArcWifiHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcWifiHostImpl* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcWifiHostImpl* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // The constructor will register an Observer with ArcBridgeService.
  ArcWifiHostImpl(content::BrowserContext* context,
                  ArcBridgeService* arc_bridge_service);

  ArcWifiHostImpl(const ArcWifiHostImpl&) = delete;
  ArcWifiHostImpl& operator=(const ArcWifiHostImpl&) = delete;

  ~ArcWifiHostImpl() override;

  // Overridden from mojom::ArcWifiHost.
  void GetWifiEnabledState(GetWifiEnabledStateCallback callback) override;
  void SetWifiEnabledState(bool is_enabled,
                           SetWifiEnabledStateCallback callback) override;
  void StartScan() override;
  void GetScanResults(GetScanResultsCallback callback) override;

  // Overridden from ash::NetworkStateHandlerObserver.
  void ScanCompleted(const ash::DeviceState* /*unused*/) override;
  void OnShuttingDown() override;
  // This method is triggered when WiFi enabled state changes, while note that
  // different from the method name suggests, normally the WiFi Device is never
  // removed in the platform in spite of WiFi enabled states.
  void DeviceListChanged() override;

  // Overridden from ConnectionObserver<mojom::ArcWifiInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  THREAD_CHECKER(thread_checker_);
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_ARC_WIFI_HOST_IMPL_H_
