// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/navigation_capture/glic_share_link_navigation_throttle.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

bool MatchesCaptureUrl(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }
  const url::Origin origin = url::Origin::Create(url);
  if (origin.opaque()) {
    return false;
  }
  const std::string urls_param =
      features::kGlicConversationSharingCaptureUrls.Get();
  for (std::string_view piece : base::SplitStringPiece(
           urls_param, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    GURL pattern_url(piece);
    if (pattern_url.is_valid() && pattern_url.SchemeIs(url::kHttpsScheme) &&
        origin.IsSameOriginWith(pattern_url) &&
        url.path().starts_with(pattern_url.path())) {
      return true;
    }
  }
  return false;
}

}  // namespace

// static
void GlicShareLinkNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(features::kGlicConversationSharing)) {
    return;
  }
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  content::WebContents* web_contents = handle.GetWebContents();
  if (!web_contents) {
    return;
  }
  if (!handle.IsInPrimaryMainFrame() ||
      !tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!GlicEnabling::IsEnabledForProfile(profile)) {
    return;
  }
  registry.AddThrottle(
      std::make_unique<GlicShareLinkNavigationThrottle>(registry));
}

GlicShareLinkNavigationThrottle::GlicShareLinkNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

GlicShareLinkNavigationThrottle::~GlicShareLinkNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GlicShareLinkNavigationThrottle::WillStartRequest() {
  return HandleRequest();
}

content::NavigationThrottle::ThrottleCheckResult
GlicShareLinkNavigationThrottle::WillRedirectRequest() {
  return HandleRequest();
}

const char* GlicShareLinkNavigationThrottle::GetNameForLogging() {
  return "GlicShareLinkNavigationThrottle";
}

void GlicShareLinkNavigationThrottle::InvokeGlic() {
  // TODO: invoke glic
}

content::NavigationThrottle::ThrottleCheckResult
GlicShareLinkNavigationThrottle::HandleRequest() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return PROCEED;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!GlicEnabling::IsEnabledForProfile(profile)) {
    return PROCEED;
  }
  if (MatchesCaptureUrl(navigation_handle()->GetURL())) {
    InvokeGlic();
  }
  return PROCEED;
}

}  // namespace glic
