// Copyright 2020 The Chromium Authors. All rights reserved.
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

// Discards a page on the UI thread.
bool DiscardPageOnUIThread(const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const contents = contents_proxy.Get();
  if (!contents)
    return false;

  auto* lifecycle_unit =
      resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
          contents);
  if (!lifecycle_unit)
    return false;

  return lifecycle_unit->DiscardTab(
      resource_coordinator::LifecycleUnitDiscardReason::URGENT);
}

}  // namespace

void PageDiscarder::DiscardPageNode(
    const PageNode* page_node,
    base::OnceCallback<void(bool)> post_discard_cb) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DiscardPageOnUIThread, page_node->GetContentsProxy()),
      std::move(post_discard_cb));
}

}  // namespace mechanism
}  // namespace performance_manager
