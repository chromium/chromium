// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_loader.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/split_tab_data.h"
#endif

namespace performance_manager {

namespace mechanism {

void PageLoader::LoadPageNode(const PageNode* page_node) {
  DCHECK(page_node);
  DCHECK_EQ(page_node->GetType(), PageType::kTab);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<content::WebContents> contents) {
                       if (contents) {
                         contents->GetController().LoadIfNecessary();
                       }
                     },
                     page_node->GetWebContents()));
}

std::vector<const PageNode*> PageLoader::GetPageNodesToLoad(
    const PageNode* page_node) {
#if BUILDFLAG(IS_ANDROID)
  // Split tabs work differently on Android, at the Java level: as a result,
  // there is no split, only return the current node.
  return {page_node};
#else
  std::vector<const PageNode*> split_nodes;
  tabs::TabInterface* const source_tab =
      tabs::TabInterface::MaybeGetFromContents(
          page_node->GetWebContents().get());
  if (!source_tab) {
    // The PageNode may not be in a TabInterface in unit tests.
    CHECK_IS_TEST();
  }
  std::optional<split_tabs::SplitTabId> split_id =
      source_tab ? source_tab->GetSplit() : std::nullopt;
  if (split_id.has_value()) {
    TabStripModel* tab_strip_model =
        source_tab->GetBrowserWindowInterface()->GetTabStripModel();
    for (tabs::TabInterface* tab :
         tab_strip_model->GetSplitData(split_id.value())->ListTabs()) {
      split_nodes.push_back(
          PerformanceManager::GetPrimaryPageNodeForWebContents(
              tab->GetContents())
              .get());
    }
  } else {
    split_nodes.push_back(page_node);
  }
  return split_nodes;
#endif
}

}  // namespace mechanism

}  // namespace performance_manager
