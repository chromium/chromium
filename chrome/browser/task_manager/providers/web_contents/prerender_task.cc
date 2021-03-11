// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/prerender_task.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace task_manager {

namespace {

gfx::ImageSkia* g_prerender_icon = nullptr;

// Returns the prerender icon or |nullptr| if the |ResourceBundle| is not ready
// yet.
gfx::ImageSkia* GetPrerenderIcon() {
  if (g_prerender_icon)
    return g_prerender_icon;

  if (!ui::ResourceBundle::HasSharedInstance())
    return nullptr;

  g_prerender_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_PRERENDER);
  return g_prerender_icon;
}

std::u16string PrefixPrerenderTitle(const std::u16string& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX, title);
}

}  // namespace

PrerenderTask::PrerenderTask(content::WebContents* web_contents)
    : RendererTask(PrefixPrerenderTitle(
                       RendererTask::GetTitleFromWebContents(web_contents)),
                   GetPrerenderIcon(),
                   web_contents) {}

PrerenderTask::~PrerenderTask() {
}

void PrerenderTask::UpdateTitle() {
  // As long as this task lives we keep prefixing its title with "Prerender:".
  set_title(PrefixPrerenderTitle(
      RendererTask::GetTitleFromWebContents(web_contents())));
}

void PrerenderTask::UpdateFavicon() {
  // As long as this task lives we keep using the prerender icon, so we ignore
  // this event.
}

}  // namespace task_manager
