// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARCVM_WORKING_SET_TRIM_EXECUTOR_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARCVM_WORKING_SET_TRIM_EXECUTOR_H_

#include <string>

#include "ash/components/arc/mojom/memory.mojom-forward.h"
#include "base/functional/callback_forward.h"
#include "base/timer/elapsed_timer.h"

namespace content {
class BrowserContext;
}

namespace arc {

enum class ArcVmReclaimType {
  kReclaimNone = 0,
  kReclaimGuestPageCaches,
  kReclaimAll,  // both guest page caches and shmem
};

class ArcVmWorkingSetTrimExecutor {
 public:
  using ResultCallback =
      base::OnceCallback<void(bool result, const std::string& failure_reason)>;

  // Asks vm_concierge to trim ARCVM's memory in the same way as TrimWorkingSet.
  // |callback| is invoked upon completion.
  // |page_limit| is the maximum number of pages to reclaim
  //             (arc::ArcSession::kNoPageLimit for no limit)
  // The function must be called on the UI thread.
  static void Trim(content::BrowserContext* context,
                   ResultCallback callback,
                   ArcVmReclaimType reclaim_type,
                   int page_limit);

 private:
  static void OnDropArcVmCaches(content::BrowserContext* context,
                                ResultCallback callback,
                                ArcVmReclaimType reclaim_type,
                                int page_limit,
                                bool result);

  static void OnArcVmMemoryGuestReclaim(
      std::unique_ptr<base::ElapsedTimer> elapsed_timer,
      ResultCallback callback,
      arc::mojom::ReclaimResultPtr result);

  static void LogErrorAndInvokeCallback(const char* error,
                                        ResultCallback callback);
};
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARCVM_WORKING_SET_TRIM_EXECUTOR_H_
