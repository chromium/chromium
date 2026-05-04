// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/guest_view_policy.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/performance_manager/policies/process_rank_policy_android.h"
#include "components/performance_manager/public/performance_manager.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace performance_manager {

void GuestViewAssociatedToWebContents(
    content::WebContents* guest_web_contents) {
#if BUILDFLAG(IS_ANDROID)
  auto page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(guest_web_contents);
  if (page_node) {
    policies::ProcessRankPolicyAndroid::GetFromGraph(page_node->GetGraph())
        ->OnGuestViewAssociated(page_node.get());
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace performance_manager
