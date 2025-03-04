// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/tool_task.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

std::u16string GetTitle(int tool_name) {
  if (tool_name == 0) {
    return std::u16string(u"");
  }

  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TOOL_PREFIX,
                                    l10n_util::GetStringUTF16(tool_name));
}

}  // namespace

gfx::ImageSkia* ToolTask::s_icon_ = nullptr;

ToolTask::ToolTask(content::WebContents* web_contents, int tool_name)
    : RendererTask(GetTitle(tool_name),
                   FetchIcon(IDR_PLUGINS_FAVICON, &s_icon_),
                   web_contents) {}

ToolTask::~ToolTask() = default;

void ToolTask::UpdateTitle() {
  // The title never needs to change.
}

void ToolTask::UpdateFavicon() {
  // The icon never needs to change.
}

}  // namespace task_manager
