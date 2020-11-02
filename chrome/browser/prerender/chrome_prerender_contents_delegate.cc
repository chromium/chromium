// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/chrome_prerender_contents_delegate.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/prerender/browser/prerender_contents.h"
#include "components/prerender/browser/prerender_histograms.h"
#include "components/prerender/browser/prerender_manager.h"
#include "content/public/browser/web_contents.h"

namespace prerender {

// static
PrerenderContents* ChromePrerenderContentsDelegate::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!prerender_manager)
    return nullptr;
  return prerender_manager->GetPrerenderContents(web_contents);
}

void ChromePrerenderContentsDelegate::OnPrerenderContentsCreated(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  TabHelpers::AttachTabHelpers(web_contents);

  // Tag the prerender contents with the task manager specific prerender tag, so
  // that it shows up in the task manager.
  task_manager::WebContentsTags::CreateForPrerenderContents(web_contents);
}

void ChromePrerenderContentsDelegate::ReleasePrerenderContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  // Clear the task manager tag we added earlier to our
  // WebContents since it's no longer a prerender contents.
  task_manager::WebContentsTags::ClearTag(web_contents);
}

}  // namespace prerender
