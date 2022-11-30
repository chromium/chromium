// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_helper.h"

#include <utility>

#include "base/time/default_clock.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace {

// The Clock that a new DIPSTabHelper will use internally. Exposed as a global
// so that browser tests (which don't call the DIPSTabHelper constructor
// directly) can inject a fake clock.
base::Clock* g_clock = nullptr;

}  // namespace

DIPSTabHelper::DIPSTabHelper(content::WebContents* web_contents,
                             DIPSService* service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSTabHelper>(*web_contents),
      service_(service),
      clock_(g_clock ? g_clock : base::DefaultClock::GetInstance()) {
  DCHECK(service_);
}

DIPSCookieMode DIPSTabHelper::GetCookieMode() const {
  return GetDIPSCookieMode(
      web_contents()->GetBrowserContext()->IsOffTheRecord(),
      service_->ShouldBlockThirdPartyCookies());
}

void DIPSTabHelper::FlushForTesting(base::OnceClosure flushed) {
  service_->storage()
      ->AsyncCall(&DIPSStorage::DoNothing)
      .Then(std::move(flushed));
}

void DIPSTabHelper::StateForURLForTesting(const GURL& url,
                                          StateForURLCallback callback) {
  service_->storage()
      ->AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(std::move(callback));
}

/* static */
base::Clock* DIPSTabHelper::SetClockForTesting(base::Clock* clock) {
  return std::exchange(g_clock, clock);
}

void DIPSTabHelper::RecordStorage(const GURL& url) {
  base::Time now = clock_->Now();
  DIPSCookieMode mode = GetCookieMode();

  service_->storage()
      ->AsyncCall(&DIPSStorage::RecordStorage)
      .WithArgs(url, now, mode);
}

void DIPSTabHelper::RecordInteraction(const GURL& url) {
  base::Time now = clock_->Now();
  DIPSCookieMode mode = GetCookieMode();

  service_->storage()
      ->AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(url, now, mode);
}

void DIPSTabHelper::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (details.type == content::CookieAccessDetails::Type::kChange) {
    RecordStorage(details.url);
  }
}

void DIPSTabHelper::OnCookiesAccessed(
    content::NavigationHandle* handle,
    const content::CookieAccessDetails& details) {
  if (details.type == content::CookieAccessDetails::Type::kChange) {
    RecordStorage(details.url);
  }
}

void DIPSTabHelper::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  const GURL& url = render_frame_host->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  RecordInteraction(url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSTabHelper);
