// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tab_helper.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace prerender {

PrerenderTabHelper::PrerenderTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), origin_(ORIGIN_NONE) {}

PrerenderTabHelper::~PrerenderTabHelper() {
}

void PrerenderTabHelper::DidFinishNavigation(
      content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }

  url_ = navigation_handle->GetURL();
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents(), NULL))
    return;
  prerender_manager->RecordNavigation(url_);
}

void PrerenderTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Determine the origin.
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (prerender_manager)
    prerender_manager->IsWebContentsPrerendering(web_contents(), &origin_);

  if (navigation_handle->IsSameDocument())
    return;

  if (!navigation_handle->IsInMainFrame())
    return;

  MainFrameUrlDidChange(navigation_handle->GetURL());
}

void PrerenderTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  MainFrameUrlDidChange(navigation_handle->GetURL());
}

void PrerenderTabHelper::MainFrameUrlDidChange(const GURL& url) {
  url_ = url;
}

PrerenderManager* PrerenderTabHelper::MaybeGetPrerenderManager() const {
  return PrerenderManagerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

base::TimeTicks PrerenderTabHelper::GetTimeTicksFromPrerenderManager() const {
  // Prerender browser tests should always have a PrerenderManager when mocking
  // out tick clock.
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (prerender_manager)
    return prerender_manager->GetCurrentTimeTicks();

  // Fall back to returning the same value as PrerenderManager would have
  // returned in production.
  return base::TimeTicks::Now();
}

bool PrerenderTabHelper::IsPrerendering() {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return false;
  return prerender_manager->IsWebContentsPrerendering(web_contents(), NULL);
}

void PrerenderTabHelper::PrerenderSwappedIn() {
  DCHECK(!IsPrerendering());
  swap_ticks_ = GetTimeTicksFromPrerenderManager();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrerenderTabHelper)

}  // namespace prerender
