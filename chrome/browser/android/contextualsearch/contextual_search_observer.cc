// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_observer.h"

#include "components/contextual_search/content/browser/contextual_search_js_api_handler.h"
#include "content/public/browser/web_contents.h"

namespace contextual_search {

ContextualSearchObserver::ContextualSearchObserver(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ContextualSearchObserver>(*web_contents) {}

ContextualSearchObserver::~ContextualSearchObserver() = default;

// static
void ContextualSearchObserver::SetHandlerForWebContents(
    content::WebContents* web_contents,
    ContextualSearchJsApiHandler* handler) {
  DCHECK(web_contents);
  DCHECK(handler);

  // Clobber any prior registered observer.
  web_contents->RemoveUserData(UserDataKey());
  CreateForWebContents(web_contents);
  auto* contextual_search_observer = FromWebContents(web_contents);
  contextual_search_observer->set_api_handler(handler);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualSearchObserver);

}  // namespace contextual_search
