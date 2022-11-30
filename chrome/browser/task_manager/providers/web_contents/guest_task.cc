// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/guest_task.h"

#include "components/guest_view/browser/guest_view_base.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

GuestTask::GuestTask(content::WebContents* web_contents)
    : RendererTask(GetCurrentTitle(web_contents),
                   GetFaviconFromWebContents(web_contents),
                   web_contents) {}

GuestTask::~GuestTask() {
}

void GuestTask::UpdateTitle() {
  set_title(GetCurrentTitle(web_contents()));
}

void GuestTask::UpdateFavicon() {
  const gfx::ImageSkia* icon = GetFaviconFromWebContents(web_contents());
  set_icon(icon ? *icon : gfx::ImageSkia());
}

Task::Type GuestTask::GetType() const {
  return Task::GUEST;
}

std::u16string GuestTask::GetCurrentTitle(
    content::WebContents* web_contents) const {
  DCHECK(web_contents);

  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromWebContents(web_contents);

  if (!guest) {
    // This can happen when an AppWindowContentsImpl is destroyed. It emits a
    // DidFinishNavigation() events to the WebContentsObservers which triggers a
    // title update in WebContentsTaskProvider. This happens before
    // WebContentsDestroyed() is emitted.
    return title();
  }

  std::u16string title = l10n_util::GetStringFUTF16(
      guest->GetTaskPrefix(),
      RendererTask::GetTitleFromWebContents(web_contents));

  return title;
}

}  // namespace task_manager

