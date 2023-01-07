// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/fenced_frame_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace task_manager {

FencedFrameTask::FencedFrameTask(content::RenderFrameHost* render_frame_host,
                                 RendererTask* embedder_task)
    : RendererTask(
          /*title=*/u"",
          /*icon=*/nullptr,
          /*subframe=*/render_frame_host),
      site_instance_(render_frame_host->GetSiteInstance()),
      embedder_task_(embedder_task) {
  set_title(GetTitle());
}

void FencedFrameTask::Activate() {
  embedder_task_->Activate();
}

const task_manager::Task* FencedFrameTask::GetParentTask() const {
  return embedder_task_;
}

void FencedFrameTask::UpdateTitle() {
  set_title(GetTitle());
}

std::u16string FencedFrameTask::GetTitle() const {
  const auto message_id = site_instance_->GetBrowserContext()->IsOffTheRecord()
                              ? IDS_TASK_MANAGER_FENCED_FRAME_INCOGNITO_PREFIX
                              : IDS_TASK_MANAGER_FENCED_FRAME_PREFIX;
  const auto title = base::UTF8ToUTF16(site_instance_->GetSiteURL().spec());
  return l10n_util::GetStringFUTF16(message_id, title);
}

}  // namespace task_manager
