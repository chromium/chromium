// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_service_impl.h"

#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

void AddForcedURLOnUIThread(scoped_refptr<history::TopSites> top_sites,
                            const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (top_sites)
    top_sites->AddForcedURL(url, base::Time::Now());
}

}  // namespace

namespace thumbnails {

ThumbnailServiceImpl::ThumbnailServiceImpl(Profile* profile)
    : top_sites_(TopSitesFactory::GetForProfile(profile)) {}

ThumbnailServiceImpl::~ThumbnailServiceImpl() {
}

bool ThumbnailServiceImpl::SetPageThumbnail(const ThumbnailingContext& context,
                                            const gfx::Image& thumbnail) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (!local_ptr)
    return false;

  return local_ptr->SetPageThumbnail(context.url, thumbnail, context.score);
}

bool ThumbnailServiceImpl::GetPageThumbnail(
    const GURL& url,
    bool prefix_match,
    scoped_refptr<base::RefCountedMemory>* bytes) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (!local_ptr)
    return false;

  return local_ptr->GetPageThumbnail(url, prefix_match, bytes);
}

void ThumbnailServiceImpl::AddForcedURL(const GURL& url) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (!local_ptr)
    return;

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::Bind(AddForcedURLOnUIThread, local_ptr, url));
}

bool ThumbnailServiceImpl::ShouldAcquirePageThumbnail(
    const GURL& url,
    ui::PageTransition transition) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);

  if (!local_ptr)
    return false;

  // Skip if the given URL is not appropriate for history.
  if (!CanAddURLToHistory(url))
    return false;
  // If the URL is not known (i.e. not a top site yet), do some extra checks.
  if (!local_ptr->IsKnownURL(url)) {
    // Skip if the top sites list is full - no point in taking speculative
    // thumbnails.
    if (local_ptr->IsNonForcedFull())
      return false;

    // Skip if the transition type is not interesting:
    // Only new segments (roughly "initial navigations", e.g. not clicks on a
    // link) can end up in TopSites (see HistoryBackend::UpdateSegments).
    // Note that for pages that are already in TopSites, we don't care about
    // the transition type, since for those we know we'll need the thumbnail.
    if (!ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
        !ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
      return false;
    }
  }

  // Skip if we don't have to update the existing thumbnail.
  history::ThumbnailScore current_score;
  if (local_ptr->GetPageThumbnailScore(url, &current_score) &&
      !current_score.ShouldConsiderUpdating()) {
    return false;
  }
  // Skip if we don't have to update the temporary thumbnail (i.e. the one
  // not yet saved).
  history::ThumbnailScore temporary_score;
  if (local_ptr->GetTemporaryPageThumbnailScore(url, &temporary_score) &&
      !temporary_score.ShouldConsiderUpdating()) {
    return false;
  }

  return true;
}

void ThumbnailServiceImpl::ShutdownOnUIThread() {
  // Since each call uses its own scoped_refptr, we can just clear the reference
  // here by assigning null. If another call is completed, it added its own
  // reference.
  top_sites_ = NULL;
}

}  // namespace thumbnails
