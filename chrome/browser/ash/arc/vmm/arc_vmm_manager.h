// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace arc {

class ArcBridgeService;

// ARCVM vmm features manager.
class ArcVmmManager : public KeyedService {
 public:
  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use ARC.
  static ArcVmmManager* GetForBrowserContext(content::BrowserContext* context);

  ArcVmmManager(content::BrowserContext* context, ArcBridgeService* bridge);

  ArcVmmManager(const ArcVmmManager&) = delete;
  ArcVmmManager& operator=(const ArcVmmManager&) = delete;

  ~ArcVmmManager() override;

  // SetSwapState change the ARCVM vmm swap state in crosvm. When swap enabled,
  // the crosvm process will be STOP and guest memory will be moved to the
  // staging memory.
  void SetSwapState(bool enable);

  void set_user_id_hash(const std::string& user_id_hash) {
    user_id_hash_ = user_id_hash;
  }

 private:
  // Accelerator target for experimental usage. Ctrl + Alt + Shift + O / P for
  // enable or disable vmm swap.
  class AcceleratorTarget;

  // Accelerator for experimental usage. Always behind the feature flag.
  std::unique_ptr<AcceleratorTarget> accelerator_;

  std::string user_id_hash_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_MANAGER_H_
