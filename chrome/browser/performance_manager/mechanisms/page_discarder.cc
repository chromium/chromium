// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {
namespace mechanism {
namespace {

bool disabled_for_testing = false;

// Discards pages on the UI thread. Returns true if at least 1 page is
// discarded.
// TODO(crbug/1241049): Returns the remaining reclaim target so
// UrgentlyDiscardMultiplePages can keep reclaiming until the reclaim target is
// met or there is no discardable page.
bool DiscardPagesOnUIThread(
    const std::vector<std::pair<WebContentsProxy, uint64_t>>& proxies_and_rss) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (disabled_for_testing)
    return false;

  bool result = false;
  for (auto proxy : proxies_and_rss) {
    content::WebContents* const contents = proxy.first.Get();
    if (!contents)
      continue;

    auto* lifecycle_unit = resource_coordinator::TabLifecycleUnitSource::
        GetTabLifecycleUnitExternal(contents);
    if (!lifecycle_unit)
      continue;

    if (lifecycle_unit->DiscardTab(
            resource_coordinator::LifecycleUnitDiscardReason::URGENT,
            /*resident_set_size_estimate=*/proxy.second)) {
      result = true;
    }
  }
  return result;
}

}  // namespace

// static
void PageDiscarder::DisableForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  disabled_for_testing = true;
}

void PageDiscarder::DiscardPageNodes(
    const std::vector<const PageNode*>& page_nodes,
    base::OnceCallback<void(bool)> post_discard_cb) {
  std::vector<std::pair<WebContentsProxy, uint64_t>> proxies_and_rss;
  proxies_and_rss.reserve(page_nodes.size());
  for (auto* page_node : page_nodes) {
    proxies_and_rss.emplace_back(page_node->GetContentsProxy(),
                                 page_node->EstimateResidentSetSize());
  }
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DiscardPagesOnUIThread, std::move(proxies_and_rss)),
      std::move(post_discard_cb));
}

}  // namespace mechanism
}  // namespace performance_manager
