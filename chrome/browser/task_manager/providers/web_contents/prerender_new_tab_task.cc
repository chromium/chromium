// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/prerender_new_tab_task.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace task_manager {

namespace {

gfx::ImageSkia* g_prerender_new_tab_icon = nullptr;

gfx::ImageSkia* GetPrerenderNewTabIcon() {
  if (g_prerender_new_tab_icon) {
    return g_prerender_new_tab_icon;
  }

  if (!ui::ResourceBundle::HasSharedInstance()) {
    return nullptr;
  }

  g_prerender_new_tab_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_PRERENDER);
  return g_prerender_new_tab_icon;
}

std::u16string PrefixPrerenderTitle(const std::u16string& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX, title);
}

}  // namespace

PrerenderNewTabTask::PrerenderNewTabTask(content::WebContents* web_contents)
    : RendererTask(PrefixPrerenderTitle(
                       RendererTask::GetTitleFromWebContents(web_contents)),
                   GetPrerenderNewTabIcon(),
                   web_contents) {}

PrerenderNewTabTask::~PrerenderNewTabTask() = default;

void PrerenderNewTabTask::UpdateTitle() {
  set_title(PrefixPrerenderTitle(
      RendererTask::GetTitleFromWebContents(web_contents())));
}

void PrerenderNewTabTask::UpdateFavicon() {
  // Keep using the prerender icon while prerendering.
}

}  // namespace task_manager
