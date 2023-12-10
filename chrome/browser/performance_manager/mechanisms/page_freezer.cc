// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_freezer.h"

#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace performance_manager {
namespace mechanism {
namespace {

// Try to freeze a page on the UI thread.
void MaybeFreezePageOnUIThread(const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const contents = contents_proxy.Get();
  if (!contents)
    return;

  content::PermissionController* permission_controller =
      contents->GetBrowserContext()->GetPermissionController();

  // Page with the notification permission shouldn't be frozen as this is a
  // strong signal that the user wants to receive updates from this page while
  // it's in background. This information isn't available in the PM graph, this
  // has to be checked on the UI thread.
  if (permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::NOTIFICATIONS,
          contents->GetPrimaryMainFrame()) ==
      blink::mojom::PermissionStatus::GRANTED) {
    return;
  }

  // A visible page should not be frozen.
  if (contents->GetVisibility() == content::Visibility::VISIBLE) {
    return;
  }

  contents->SetPageFrozen(true);
}

void UnfreezePageOnUIThread(const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const content = contents_proxy.Get();
  if (!content)
    return;

  // A visible page is automatically unfrozen.
  if (content->GetVisibility() == content::Visibility::VISIBLE)
    return;

  content->SetPageFrozen(false);
}

}  // namespace

void PageFreezer::MaybeFreezePageNode(const PageNode* page_node) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MaybeFreezePageOnUIThread,
                                page_node->GetContentsProxy()));
}

void PageFreezer::UnfreezePageNode(const PageNode* page_node) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&UnfreezePageOnUIThread, page_node->GetContentsProxy()));
}

}  // namespace mechanism
}  // namespace performance_manager
