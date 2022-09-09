// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_tab_helper.h"

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace prerender {

NoStatePrefetchTabHelper::NoStatePrefetchTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<NoStatePrefetchTabHelper>(*web_contents) {}

NoStatePrefetchTabHelper::~NoStatePrefetchTabHelper() = default;

void NoStatePrefetchTabHelper::PrimaryPageChanged(content::Page& page) {
  if (page.GetMainDocument().IsErrorDocument())
    return;

  NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!no_state_prefetch_manager)
    return;
  if (no_state_prefetch_manager->IsWebContentsPrefetching(web_contents()))
    return;
  no_state_prefetch_manager->RecordNavigation(
      page.GetMainDocument().GetLastCommittedURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NoStatePrefetchTabHelper);

}  // namespace prerender
