// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/printing_task.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

std::u16string PrefixPrintTitle(const std::u16string& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX, title);
}

}  // namespace

PrintingTask::PrintingTask(content::WebContents* web_contents)
    : RendererTask(
          PrefixPrintTitle(RendererTask::GetTitleFromWebContents(web_contents)),
          RendererTask::GetFaviconFromWebContents(web_contents),
          web_contents) {}

PrintingTask::~PrintingTask() {
}

void PrintingTask::UpdateTitle() {
  set_title(PrefixPrintTitle(GetTitleFromWebContents(web_contents())));
}

void PrintingTask::UpdateFavicon() {
  const gfx::ImageSkia* icon =
      RendererTask::GetFaviconFromWebContents(web_contents());
  set_icon(icon ? *icon : gfx::ImageSkia());
}

}  // namespace task_manager
