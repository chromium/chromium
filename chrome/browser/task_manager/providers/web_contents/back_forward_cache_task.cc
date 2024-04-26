// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/back_forward_cache_task.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::u16string GetTaskTitle(content::RenderFrameHost* render_frame_host,
                            task_manager::RendererTask* parent_task) {
  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();

  const bool is_incognito =
      site_instance->GetBrowserContext()->IsOffTheRecord();

  // TODO(crbug.com/40775860): Display the page title instead of the site URL
  // for main frames.
  const GURL& site_url = site_instance->GetSiteURL();
  const std::u16string name = base::UTF8ToUTF16(site_url.spec());

  int message_id;
  if (parent_task == nullptr) {
    message_id = is_incognito
                     ? IDS_TASK_MANAGER_BACK_FORWARD_CACHE_INCOGNITO_PREFIX
                     : IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX;
  } else {
    message_id =
        is_incognito
            ? IDS_TASK_MANAGER_BACK_FORWARD_CACHE_INCOGNITO_SUBFRAME_PREFIX
            : IDS_TASK_MANAGER_BACK_FORWARD_CACHE_SUBFRAME_PREFIX;
  }
  return l10n_util::GetStringFUTF16(message_id, name);
}

}  // anonymous namespace

namespace task_manager {

BackForwardCacheTask::BackForwardCacheTask(
    content::RenderFrameHost* render_frame_host,
    RendererTask* parent_task,
    WebContentsTaskProvider* task_provider)
    : RendererTask(
          GetTaskTitle(render_frame_host, parent_task),
          nullptr,  // TODO(crbug.com/40775860): Set Favicon for main frames.
          render_frame_host),
      parent_task_(parent_task),
      task_provider_(task_provider) {}

// For the top level BackForwardCacheTask |parent_task_| is nullptr.
Task* BackForwardCacheTask::GetParentTask() const {
  return parent_task_ ? parent_task_
                      : task_provider_->GetTaskOfFrame(
                            web_contents()->GetPrimaryMainFrame());
}

// The top page calls the default Activate().
void BackForwardCacheTask::Activate() {
  if (parent_task_) {
    parent_task_->Activate();
  } else {
    RendererTask::Activate();
  }
}

// BackForwardCache entries are frozen and must not have any updates.
// However, they are associated with the same WebContents as the active page
// and receive these updates when that page changes.
void BackForwardCacheTask::UpdateTitle() {}

void BackForwardCacheTask::UpdateFavicon() {}

}  // namespace task_manager
