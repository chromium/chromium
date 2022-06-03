// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_loader.h"

#include "components/performance_manager/public/decorators/tab_properties_decorator.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

namespace mechanism {

namespace {

// Load a page on the UI thread.
void LoadPageOnUIThread(const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const contents = contents_proxy.Get();
  if (!contents)
    return;

  contents->GetController().LoadIfNecessary();
}

}  // namespace

void PageLoader::LoadPageNode(const PageNode* page_node) {
  DCHECK(page_node);
  DCHECK(TabPropertiesDecorator::Data::FromPageNode(page_node)->IsInTabStrip());
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadPageOnUIThread, page_node->GetContentsProxy()));
}

}  // namespace mechanism

}  // namespace performance_manager
