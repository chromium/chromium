// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_helper.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_storage.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace {

inline void UmaHistogramTimeToInteraction(base::TimeDelta sample,
                                          DIPSCookieMode mode) {
  // Any changes here need to be reflected in DIPSCookieMode in
  // tools/metrics/histograms/metadata/others/histograms.xml
  const char* name;
  switch (mode) {
    case DIPSCookieMode::kStandard:
      name = "Privacy.DIPS.TimeFromStorageToInteraction.Standard";
      break;
    case DIPSCookieMode::kOffTheRecord:
      name = "Privacy.DIPS.TimeFromStorageToInteraction.OffTheRecord";
      break;
    case DIPSCookieMode::kBlock3PC:
      name = "Privacy.DIPS.TimeFromStorageToInteraction.Block3PC";
      break;
    case DIPSCookieMode::kOffTheRecord_Block3PC:
      name = "Privacy.DIPS.TimeFromStorageToInteraction.OffTheRecord_Block3PC";
      break;
    default:
      NOTREACHED() << "Invalid DIPSCookieMode: " << mode;
      return;
  }

  base::UmaHistogramCustomTimes(name, sample,
                                /*min=*/base::TimeDelta(),
                                /*max=*/base::Days(7), 100);
}

inline void UmaHistogramTimeToStorage(base::TimeDelta sample,
                                      DIPSCookieMode mode) {
  // Any changes here need to be reflected in DIPSCookieMode in
  // tools/metrics/histograms/metadata/others/histograms.xml
  const char* name;
  switch (mode) {
    case DIPSCookieMode::kStandard:
      name = "Privacy.DIPS.TimeFromInteractionToStorage.Standard";
      break;
    case DIPSCookieMode::kOffTheRecord:
      name = "Privacy.DIPS.TimeFromInteractionToStorage.OffTheRecord";
      break;
    case DIPSCookieMode::kBlock3PC:
      name = "Privacy.DIPS.TimeFromInteractionToStorage.Block3PC";
      break;
    case DIPSCookieMode::kOffTheRecord_Block3PC:
      name = "Privacy.DIPS.TimeFromInteractionToStorage.OffTheRecord_Block3PC";
      break;
    default:
      NOTREACHED() << "Invalid DIPSCookieMode: " << mode;
      return;
  }

  base::UmaHistogramCustomTimes(name, sample,
                                /*min=*/base::TimeDelta(),
                                /*max=*/base::Days(7), 100);
}

// The Clock that a new DIPSTabHelper will use internally. Exposed as a global
// so that browser tests (which don't call the DIPSTabHelper constructor
// directly) can inject a fake clock.
base::Clock* g_clock = nullptr;

}  // namespace

const char* DIPSCookieModeToString(DIPSCookieMode mode) {
  switch (mode) {
    case DIPSCookieMode::kStandard:
      return "Standard";
    case DIPSCookieMode::kOffTheRecord:
      return "OffTheRecord";
    case DIPSCookieMode::kBlock3PC:
      return "Block3PC";
    case DIPSCookieMode::kOffTheRecord_Block3PC:
      return "OffTheRecord_Block3PC";
  }
}

std::ostream& operator<<(std::ostream& os, DIPSCookieMode mode) {
  return os << DIPSCookieModeToString(mode);
}

DIPSTabHelper::DIPSTabHelper(content::WebContents* web_contents,
                             DIPSService* service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSTabHelper>(*web_contents),
      service_(service),
      clock_(g_clock ? g_clock : base::DefaultClock::GetInstance()) {
  DCHECK(service_);
}

DIPSCookieMode DIPSTabHelper::GetCookieMode() const {
  bool isOTR = web_contents()->GetBrowserContext()->IsOffTheRecord();
  bool block3PC = service_->ShouldBlockThirdPartyCookies();
  if (isOTR) {
    if (block3PC) {
      return DIPSCookieMode::kOffTheRecord_Block3PC;
    }
    return DIPSCookieMode::kOffTheRecord;
  } else if (block3PC) {
    return DIPSCookieMode::kBlock3PC;
  }
  return DIPSCookieMode::kStandard;
}

DIPSState DIPSTabHelper::StateForURL(const GURL& url) {
  return service_->storage()->Read(url);
}

/* static */
base::Clock* DIPSTabHelper::SetClockForTesting(base::Clock* clock) {
  return std::exchange(g_clock, clock);
}

void DIPSTabHelper::MaybeRecordStorage(const GURL& url) {
  DIPSState state = StateForURL(url);
  if (state.site_storage_time()) {
    // We want the time that storage was first written, so don't overwrite the
    // existing timestamp.
    return;
  }

  base::Time now = clock_->Now();
  if (state.user_interaction_time()) {
    // First storage, but previous interaction.
    UmaHistogramTimeToStorage(now - state.user_interaction_time().value(),
                              GetCookieMode());
  }

  state.set_site_storage_time(now);
}

void DIPSTabHelper::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (details.type == content::CookieAccessDetails::Type::kChange) {
    MaybeRecordStorage(details.url);
  }
}

void DIPSTabHelper::OnCookiesAccessed(
    content::NavigationHandle* handle,
    const content::CookieAccessDetails& details) {
  if (details.type == content::CookieAccessDetails::Type::kChange) {
    MaybeRecordStorage(details.url);
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

void DIPSTabHelper::RecordInteraction(const GURL& url) {
  DIPSState state = StateForURL(url);

  base::Time now = clock_->Now();
  if (!state.user_interaction_time()) {
    // First interaction on site.
    if (state.site_storage_time()) {
      // Site previously wrote to storage. Record metric for the time delay
      // between storage and interaction.
      UmaHistogramTimeToInteraction(now - state.site_storage_time().value(),
                                    GetCookieMode());
    }
  }

  // Unlike for storage, we want to know the time of the most recent user
  // interaction, so overwrite any existing timestamp. (If interaction happened
  // a long time ago, it may no longer be relevant.)
  state.set_user_interaction_time(now);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSTabHelper);
