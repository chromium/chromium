// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_BRIDGE_H_

#include "ash/components/arc/mojom/system_state.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles ARC system state update from ARC side.
class ArcSystemStateBridge : public KeyedService,
                             public mojom::SystemStateHost {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Observer for ARC SystemAppRunningState change.
    virtual void OnArcSystemAppRunningStateChange(
        const mojom::SystemAppRunningState& state) {}

   protected:
    ~Observer() override = default;
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSystemStateBridge* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcSystemStateBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  static void EnsureFactoryBuilt();

  ArcSystemStateBridge(content::BrowserContext* context,
                       ArcBridgeService* bridge_service);
  ArcSystemStateBridge(const ArcSystemStateBridge&) = delete;
  ArcSystemStateBridge& operator=(const ArcSystemStateBridge&) = delete;
  ~ArcSystemStateBridge() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const mojom::SystemAppRunningState& system_app_running_state() {
    return *state_;
  }

  // mojom::SystemStateHost override:
  void UpdateAppRunningState(mojom::SystemAppRunningStatePtr state) override;

 private:
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  mojom::SystemAppRunningStatePtr state_;

  const raw_ptr<Profile> profile_;

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<ArcSystemStateBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_BRIDGE_H_
