// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_web_contents_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

void PrerenderWebContentsDelegateImpl::PrerenderWebContentsCreated(
    content::WebContents* prerender_web_contents) {
  TabHelpers::AttachTabHelpers(prerender_web_contents);
#if BUILDFLAG(IS_ANDROID)
  RequestDesktopSiteWebContentsObserverAndroid::CreateForWebContents(
      prerender_web_contents);
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extensions::GetViewType(prerender_web_contents) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(prerender_web_contents,
                            extensions::mojom::ViewType::kTabContents);
  }
  extensions::WebNavigationTabObserver::CreateForWebContents(
      prerender_web_contents);
  extensions::TabHelper::CreateForWebContents(prerender_web_contents);
#endif

  // Tag the prerender new tab contents so that it shows up in the task manager.
  task_manager::WebContentsTags::CreateForPrerenderNewTabContents(
      prerender_web_contents);
}

void PrerenderWebContentsDelegateImpl::PrerenderWebContentsReleased(
    content::WebContents* prerender_web_contents) {
  // Clear the prerender tag so the WebContents can be re-tagged as a regular
  // tab after activation.
  task_manager::WebContentsTags::ClearTag(prerender_web_contents);
}
