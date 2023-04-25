// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/web_app_task.h"

#include <string>

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace task_manager {

WebAppTask::WebAppTask(content::WebContents* web_contents)
    : RendererTask(GetPrefixedTitle(web_contents),
                   RendererTask::GetFaviconFromWebContents(web_contents),
                   web_contents) {}

WebAppTask::~WebAppTask() = default;

void WebAppTask::UpdateTitle() {
  set_title(GetPrefixedTitle(web_contents()));
}

void WebAppTask::UpdateFavicon() {
  const gfx::ImageSkia* icon =
      RendererTask::GetFaviconFromWebContents(web_contents());
  set_icon(icon ? *icon : gfx::ImageSkia());
}

std::u16string WebAppTask::GetPrefixedTitle(
    content::WebContents* web_contents) const {
  std::u16string title = RendererTask::GetTitleFromWebContents(web_contents);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_APP_PREFIX, title);
}

}  // namespace task_manager
