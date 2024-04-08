// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_

#include <string>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_swap_scheduler.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace arc {

class ArcBridgeService;

enum class SwapState {
  ENABLE,
  FORCE_ENABLE,
  DISABLE,
};

// ARCVM vmm features manager.
class ArcVmmManager : public KeyedService,
                      public arc::ConnectionObserver<arc::mojom::AppInstance>,
                      public ash::ConciergeClient::VmObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnArcVmSwappingOut() {}
    virtual void OnArcVmSwappingIn() {}

   protected:
    ~Observer() override = default;
  };

  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use ARC.
  static ArcVmmManager* GetForBrowserContext(content::BrowserContext* context);
  static ArcVmmManager* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcVmmManager(content::BrowserContext* context, ArcBridgeService* bridge);

  ArcVmmManager(const ArcVmmManager&) = delete;
  ArcVmmManager& operator=(const ArcVmmManager&) = delete;

  ~ArcVmmManager() override;

  // SetSwapState change the ARCVM vmm swap state in crosvm. When swap enabled,
  // the crosvm process will be STOP and guest memory will be moved to the
  // staging memory.
  void SetSwapState(SwapState state);

  // Is the ARCVM on "swapped" state. If it's true, the ARC app launch maybe
  // slower than usual.
  bool IsSwapped() const;

  void set_user_id_hash(const std::string& user_id_hash) {
    user_id_hash_ = user_id_hash;
  }

  static void EnsureFactoryBuilt();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // arc::ConnectionObserver:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // ash::ConciergeClient::VmObserver override:
  void OnVmSwapping(
      const vm_tools::concierge::VmSwappingSignal& signal) override;

 private:
  friend class ArcVmmManagerTest;
  friend class ArcVmmManagerBrowserTest;
  // Accelerator target for experimental usage. Ctrl + Alt + Shift + O / P for
  // enable or disable vmm swap.
  class AcceleratorTarget;

  void SendSwapRequest(vm_tools::concierge::SwapOperation operation,
                       base::OnceClosure success_callback);

  // Wrapped function of `SendSwapRequest`. Verify if the latest operation still
  // match the calling operation. If so, pass the params to SendSwapRequest, or
  // do nothing.
  // Prefer use it in the chain of callbacks. Before enable vmm swap, the memory
  // shrink may need take several minutes. During this time, if the "disable"
  // request come, we need make sure not send the "enable" request after finish
  // memory shrink.
  void VerifyThenSendSwapRequest(vm_tools::concierge::SwapOperation operation,
                                 base::OnceClosure success_callback);

  void SendAggressiveBalloonRequest(bool enable,
                                    base::OnceClosure success_callback);

  // Wrapped function of `SendAggressiveBalloonRequest`. Verify if the latest
  // operation still match the calling operation. If so, pass the params to
  // SendAggressiveBalloonRequest, or do nothing.
  // Prefer use it in the chain of callbacks, as the same with
  // `VerifyThenSendSwapRequest`.
  void VerifyThenSendAggressiveBalloonRequest(
      bool enable,
      base::OnceClosure success_callback);

  void PostWithSwapDelay(base::OnceClosure callback);

  // Called by `SendSwapRequest` and should not be called by other caller.
  // Enable aggressive balloon and reclaim ARCVM guest memory.
  // Shrink memory before enable swap. The function send enable swap request
  // after shrink success.
  void ShrinkArcVmMemoryAndEnableSwap(
      vm_tools::concierge::SwapOperation requested_operation);

  // Called by callback from `ShrinkArcVmMemoryAndEnableSwap` and should not be
  // called by other caller. Update shrink result.
  void SetShrinkResult(bool success);

  SwapState latest_swap_state_ = SwapState::DISABLE;

  // List of observers.
  base::ObserverList<Observer> observer_list_;

  // Log the time stamp and result of last shrink memory request.
  std::optional<base::Time> last_shrink_timestamp_;
  std::optional<bool> last_shrink_result_;

  // Repeat timer for checking and trimming ARCVM memory regularly. According
  // current design in concierge, if the vmm swap status is enabled, the vmm
  // manager needs to go through the "enable" process (i.e. trim memory, set
  // aggressive balloon, send enable vmm swap dbus request) once an hour.
  base::OneShotTimer enabled_state_heartbeat_timer_;

  // The default delay from swap enabled and swap out. Basically it's used for
  // keyboard swap. In finch, it will be replaced by the flag parameter.
  base::TimeDelta swap_out_delay_ = base::Seconds(3);

  // Accelerator for experimental usage. Always behind the feature flag.
  std::unique_ptr<AcceleratorTarget> accelerator_;

  // Swap request scheduler for experimental usage. Always behind the feature
  // flag and parameters.
  std::unique_ptr<ArcVmmSwapScheduler> scheduler_;

  bool arc_connected_ = false;

  std::string user_id_hash_;

  base::RepeatingCallback<
      void(ArcVmWorkingSetTrimExecutor::ResultCallback, ArcVmReclaimType, int)>
      trim_call_;

  raw_ptr<content::BrowserContext> context_ = nullptr;
  raw_ptr<ArcBridgeService> bridge_service_ = nullptr;

  base::ScopedObservation<
      ConnectionHolder<mojom::AppInstance, mojom::AppHost>,
      ConnectionHolder<mojom::AppInstance, mojom::AppHost>::Observer>
      app_instance_observation_{this};

  base::ScopedObservation<ash::ConciergeClient,
                          ash::ConciergeClient::VmObserver>
      concierge_observation_{this};

  base::WeakPtrFactory<ArcVmmManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_
