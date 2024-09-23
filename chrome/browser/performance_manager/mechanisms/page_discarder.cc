// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {
namespace mechanism {
namespace {

bool disabled_for_testing = false;

using WebContentsAndPmf =
    std::pair<base::WeakPtr<content::WebContents>, uint64_t>;

// Discards pages on the UI thread. Returns true if at least 1 page is
// discarded.
// TODO(crbug.com/40194498): Returns the remaining reclaim target so
// UrgentlyDiscardMultiplePages can keep reclaiming until the reclaim target is
// met or there is no discardable page.
std::vector<PageDiscarder::DiscardEvent> DiscardPagesOnUIThread(
    const std::vector<WebContentsAndPmf>& web_contents_and_pmf,
    resource_coordinator::LifecycleUnitDiscardReason discard_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<PageDiscarder::DiscardEvent> discard_events;

  if (disabled_for_testing)
    return discard_events;

  for (const auto& [contents, memory_footprint_estimate] :
       web_contents_and_pmf) {
    if (!contents)
      continue;

    auto* lifecycle_unit = resource_coordinator::TabLifecycleUnitSource::
        GetTabLifecycleUnitExternal(contents.get());
    if (!lifecycle_unit)
      continue;

    if (lifecycle_unit->DiscardTab(discard_reason, memory_footprint_estimate)) {
      discard_events.emplace_back(base::TimeTicks::Now(),
                                  memory_footprint_estimate);
    }
  }
  return discard_events;
}

}  // namespace

// static
void PageDiscarder::DisableForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  disabled_for_testing = true;
}

void PageDiscarder::DiscardPageNodes(
    const std::vector<const PageNode*>& page_nodes,
    resource_coordinator::LifecycleUnitDiscardReason discard_reason,
    base::OnceCallback<void(const std::vector<DiscardEvent>&)>
        post_discard_cb) {
  std::vector<WebContentsAndPmf> web_contents_and_pmf;
  web_contents_and_pmf.reserve(page_nodes.size());
  for (const auto* page_node : page_nodes) {
    web_contents_and_pmf.emplace_back(
        page_node->GetWebContents(),
        user_tuning::GetDiscardedMemoryEstimateForPage(page_node));
  }
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DiscardPagesOnUIThread, std::move(web_contents_and_pmf),
                     discard_reason),
      std::move(post_discard_cb));
}

}  // namespace mechanism
}  // namespace performance_manager
