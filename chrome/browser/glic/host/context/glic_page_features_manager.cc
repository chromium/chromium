// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_page_features_manager.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace glic {

DEFINE_USER_DATA(GlicPageFeaturesManager);

GlicPageFeaturesManager::GlicPageFeaturesManager(tabs::TabInterface* tab)
    : content::WebContentsObserver(tab->GetContents()), tab_(tab) {
  registration_.emplace(tab->GetUnownedUserDataHost(), *this);
  tab_subscription_ = tab->RegisterWillDiscardContents(base::BindRepeating(
      &GlicPageFeaturesManager::WillDiscardContents, base::Unretained(this)));
}

GlicPageFeaturesManager::~GlicPageFeaturesManager() = default;

const std::vector<mojom::LightweightPageFeature>&
GlicPageFeaturesManager::GetFeatures() const {
  return cached_features_;
}

// static
GlicPageFeaturesManager* GlicPageFeaturesManager::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

void GlicPageFeaturesManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return;
  }

  // Cross-document navigations are handled by PrimaryPageChanged.
  if (!navigation_handle->IsSameDocument()) {
    return;
  }

  // For same document navigation we still want to re-check since it might
  // be an SPA transition (like YouTube usually is).
  cached_features_.clear();
  check_timer_.Stop();
  MaybeScheduleCheck();
}

void GlicPageFeaturesManager::PrimaryPageChanged(content::Page& page) {
  cached_features_.clear();
  check_timer_.Stop();
  MaybeScheduleCheck();
}

void GlicPageFeaturesManager::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
  cached_features_.clear();
  check_timer_.Stop();
  MaybeScheduleCheck();
}

void GlicPageFeaturesManager::MaybeScheduleCheck() {
  if (!base::FeatureList::IsEnabled(features::kGlicSummarizeVideoSuggestion)) {
    return;
  }

  auto* rfh = tab_->GetContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return;
  }

  const GURL& url = rfh->GetLastCommittedURL();
  if (url.DomainIs("youtube.com") && url.path().starts_with("/watch")) {
    check_count_ = 0;
    ScheduleCheck();
  }
}

void GlicPageFeaturesManager::ScheduleCheck() {
  check_timer_.Start(FROM_HERE, kCheckDelay,
                     base::BindOnce(&GlicPageFeaturesManager::RunCheck,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void GlicPageFeaturesManager::RunCheck() {
  auto* rfh = tab_->GetContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return;
  }
  // checking for a button with label 'Ask' as a fallback for pre-stable
  // channels.
  std::u16string script =
      u"(function() { return !!(document.querySelector('#flexible-item-buttons "
      u"button[aria-label=\"Ask\"]')); })();";
  rfh->ExecuteJavaScriptInIsolatedWorld(
      script,
      base::BindOnce(&GlicPageFeaturesManager::OnCheckResult,
                     weak_ptr_factory_.GetWeakPtr()),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void GlicPageFeaturesManager::OnCheckResult(base::Value result) {
  if (result.is_bool() && result.GetBool()) {
    base::UmaHistogramEnumeration(
        "Glic.YoutubeSummarizeVideoZSS.Events",
        check_count_ == 0
            ? YoutubeSummarizeVideoZSS::kButtonFoundOnFirstCheck
            : YoutubeSummarizeVideoZSS::kButtonFoundOnSecondCheck);
    if (!std::ranges::contains(
            cached_features_,
            mojom::LightweightPageFeature::kYtAskButtonPresent)) {
      cached_features_.push_back(
          mojom::LightweightPageFeature::kYtAskButtonPresent);
    }
  } else if (check_count_ < kMaxRetries) {
    check_count_++;
    ScheduleCheck();
  } else {
    base::UmaHistogramEnumeration(
        "Glic.YoutubeSummarizeVideoZSS.Events",
        YoutubeSummarizeVideoZSS::kButtonNotFoundAfterAllChecks);
  }
}

}  // namespace glic
