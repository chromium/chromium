// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_swap_scheduler.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace arc {

class ArcBridgeService;

enum class SwapState {
  ENABLE,
  ENABLE_WITH_SWAPOUT,
  DISABLE,
};

// ARCVM vmm features manager.
class ArcVmmManager : public KeyedService {
 public:
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

  void set_user_id_hash(const std::string& user_id_hash) {
    user_id_hash_ = user_id_hash;
  }

  static void EnsureFactoryBuilt();

 private:
  // Accelerator target for experimental usage. Ctrl + Alt + Shift + O / P for
  // enable or disable vmm swap.
  class AcceleratorTarget;

  void SendSwapRequest(vm_tools::concierge::SwapOperation operation,
                       base::OnceClosure success_callback);

  void PostWithSwapDelay(base::OnceClosure callback);

  // The default delay from swap enabled and swap out. Basically it's used for
  // keyboard swap. In finch, it will be replaced by the flag parameter.
  base::TimeDelta swap_out_delay_ = base::Seconds(3);

  // Accelerator for experimental usage. Always behind the feature flag.
  std::unique_ptr<AcceleratorTarget> accelerator_;

  // Swap request scheduler for experimental usage. Always behind the feature
  // flag and parameters.
  std::unique_ptr<ArcVmmSwapScheduler> scheduler_;

  std::string user_id_hash_;

  base::WeakPtrFactory<ArcVmmManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_
