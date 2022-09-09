// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/prerender_task.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace task_manager {

PrerenderTask::PrerenderTask(content::RenderFrameHost* render_frame_host,
                             WebContentsTaskProvider* task_provider)
    : RendererTask(
          /*title=*/u"",
          /*icon=*/nullptr,
          /*subframe=*/render_frame_host),
      render_frame_host_(render_frame_host),
      task_provider_(task_provider) {
  DCHECK(render_frame_host_);
  DCHECK_EQ(render_frame_host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);
  set_title(GetTitle());
}

PrerenderTask::~PrerenderTask() = default;

const Task* PrerenderTask::GetParentTask() const {
  return task_provider_->GetTaskOfFrame(web_contents()->GetPrimaryMainFrame());
}

void PrerenderTask::UpdateTitle() {
  set_title(GetTitle());
}

std::u16string PrerenderTask::GetTitle() const {
  const auto title =
      base::UTF8ToUTF16(render_frame_host_->GetLastCommittedURL().spec());
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX, title);
}

}  // namespace task_manager
