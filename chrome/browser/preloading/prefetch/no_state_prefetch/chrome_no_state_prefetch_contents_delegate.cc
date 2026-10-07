// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

namespace prerender {

// static
NoStatePrefetchContents* ChromeNoStatePrefetchContentsDelegate::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!no_state_prefetch_manager) {
    return nullptr;
  }
  return no_state_prefetch_manager->GetNoStatePrefetchContents(web_contents);
}

void ChromeNoStatePrefetchContentsDelegate::OnNoStatePrefetchContentsCreated(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  TabHelpers::AttachTabHelpers(web_contents);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extensions::GetViewType(web_contents) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(web_contents,
                            extensions::mojom::ViewType::kTabContents);
  }
  extensions::TabHelper::CreateForWebContents(web_contents);
#endif

  // Tag the NoStatePrefetch contents with the task manager specific prerender
  // tag, so that it shows up in the task manager.
  task_manager::WebContentsTags::CreateForNoStatePrefetchContents(web_contents);
}

void ChromeNoStatePrefetchContentsDelegate::ReleaseNoStatePrefetchContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  // Clear the task manager tag we added earlier to our
  // WebContents since it's no longer a NoStatePrefetch contents.
  task_manager::WebContentsTags::ClearTag(web_contents);
}

}  // namespace prerender
