// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/tab_contents_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"

namespace task_manager {

namespace {

bool HostsExtension(content::WebContents* web_contents) {
  DCHECK(web_contents);
  return web_contents->GetLastCommittedURL().SchemeIs(
      extensions::kExtensionScheme);
}

}  // namespace

TabContentsTask::TabContentsTask(content::WebContents* web_contents)
    : RendererTask(std::u16string(),
                   RendererTask::GetFaviconFromWebContents(web_contents),
                   web_contents) {
  set_title(GetCurrentTitle());
}

TabContentsTask::~TabContentsTask() {
}

void TabContentsTask::UpdateTitle() {
  set_title(GetCurrentTitle());
}

void TabContentsTask::UpdateFavicon() {
  const gfx::ImageSkia* icon =
      RendererTask::GetFaviconFromWebContents(web_contents());
  set_icon(icon ? *icon : gfx::ImageSkia());
}

Task::Type TabContentsTask::GetType() const {
  // A tab that loads an extension URL is considered to be an extension even
  // though it's tracked as a TabContentsTask.
  return HostsExtension(web_contents()) ? Task::EXTENSION : Task::RENDERER;
}

std::u16string TabContentsTask::GetCurrentTitle() const {
  // Check if the URL is an app and if the tab is hoisting an extension.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  GURL url = web_contents()->GetLastCommittedURL();

  bool is_app = process_map->Contains(GetChildProcessUniqueID()) &&
      extension_registry->enabled_extensions().GetAppByURL(url) != nullptr;
  bool is_extension = HostsExtension(web_contents());
  bool is_incognito = profile->IsOffTheRecord();

  std::u16string tab_title =
      RendererTask::GetTitleFromWebContents(web_contents());

  // Fall back to the URL if the title is empty.
  return PrefixRendererTitle(tab_title,
                             is_app,
                             is_extension,
                             is_incognito,
                             false);  // is_background.
}

}  // namespace task_manager
