// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_cache_handler.h"

#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace page_content_annotations {

PageContentCacheHandler::PageContentCacheHandler(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& profile_path)
    : page_content_cache_(
          std::make_unique<PageContentCache>(os_crypt_async, profile_path)) {}

PageContentCacheHandler::~PageContentCacheHandler() = default;

void PageContentCacheHandler::OnTabClosed(int64_t tab_id) {
  page_content_cache_->RemovePageContentForTab(tab_id);
}

void PageContentCacheHandler::OnVisibilityChanged(
    std::optional<int64_t> tab_id,
    content::WebContents* web_contents,
    content::Visibility visibility,
    std::optional<optimization_guide::proto::AnnotatedPageContent> result) {
  if (!tab_id || web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return;
  }
  if (visibility != content::Visibility::HIDDEN || !result) {
    return;
  }
  // Even if background trigger is enabled, update the cache with available page
  // contents. This is to avoid losing context if tab was killed as soon as it
  // was hidden. If extraction succeeds, then cache would be updated again in
  // ProcessPageContentExtraction().

  // TODO(crbug.com/440643544): Pass in the extraction timestamp.
  page_content_cache_->CachePageContent(
      *tab_id, web_contents->GetLastCommittedURL(),
      web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
      base::Time::Now(), *result);
}

void PageContentCacheHandler::OnNewNavigation(
    std::optional<int64_t> tab_id,
    content::WebContents* web_contents) {
  if (!tab_id || web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return;
  }
  // Delete cached contents for the tab_id when page is updated.
  page_content_cache_->RemovePageContentForTab(*tab_id);
}

void PageContentCacheHandler::ProcessPageContentExtraction(
    std::optional<int64_t> tab_id,
    content::WebContents* web_contents,
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  if (!tab_id || !web_contents ||
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return;
  }

  // This method only handles the case when extraction finishes when tab is
  // already backgrounded. We do not cache contents for active tab since it can
  // be extracted on demand.
  if (web_contents->GetVisibility() == content::Visibility::HIDDEN) {
    page_content_cache_->CachePageContent(
        *tab_id, web_contents->GetLastCommittedURL(),
        web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
        base::Time::Now(), page_content);
  }
}

}  // namespace page_content_annotations
