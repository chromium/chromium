// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_loader.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

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

}  // namespace mechanism

}  // namespace performance_manager
