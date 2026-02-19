// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_navigation_throttle.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/profiles/profile.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#endif
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace glic {

// Retrieves the GURL for the Web Continuity URL, defaulting to the Guest URL
// with "/continue" appended.
GURL GetGlicWebContinuityUrl() {
  static const base::NoDestructor<GURL> continuity_url([]() {
    std::string continuity_url_str = features::kGlicWebContinuityUrl.Get();
    if (!continuity_url_str.empty()) {
      GURL url(continuity_url_str);
      if (url.is_valid()) {
        return url;
      }
    }

    std::string guest_url_str = features::kGlicGuestURL.Get();
    if (!guest_url_str.empty()) {
      GURL url(guest_url_str);
      if (url.is_valid()) {
        return GURL(url.spec() + "/continue");
      }
    }
    return GURL();
  }());
  return *continuity_url;
}

// Retrieves the GURL for the Web Continuity Originating Host, defaulting to the
// Guest URL's origin.
GURL GetGlicWebContinuityOriginatingHostUrl() {
  static const base::NoDestructor<GURL> continuity_url([]() {
    std::string continuity_url_str =
        features::kGlicWebContinuityOriginatingHost.Get();
    if (!continuity_url_str.empty()) {
      GURL url(continuity_url_str);
      if (url.is_valid()) {
        return url;
      }
    }

    std::string guest_url_str = features::kGlicGuestURL.Get();
    if (!guest_url_str.empty()) {
      GURL url(guest_url_str);
      if (url.is_valid()) {
        return url.GetWithEmptyPath();
      }
    }
    return GURL();
  }());
  return *continuity_url;
}

// static
void GlicNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(features::kGlicWebContinuity)) {
    return;
  }
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  content::WebContents* web_contents = handle.GetWebContents();
  if (!web_contents) {
    return;
  }
  if (!GlicEnabling::IsEnabledForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    return;
  }
  if (!handle.IsInPrimaryMainFrame() ||
      !tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    return;
  }
  const GURL& url = handle.GetURL();
  const GURL& web_continuity_url = GetGlicWebContinuityUrl();
  const GURL& web_continuity_originating_host_url =
      GetGlicWebContinuityOriginatingHostUrl();

  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  bool url_match = url.ReplaceComponents(replacements) ==
                   web_continuity_url.ReplaceComponents(replacements);

  if (url_match) {
    const std::optional<url::Origin>& initiator_origin =
        handle.GetInitiatorOrigin();
    if (initiator_origin.has_value()) {
      bool initiator_host_match =
          initiator_origin->GetURL().scheme() ==
              web_continuity_originating_host_url.scheme() &&
          initiator_origin->GetURL().host() ==
              web_continuity_originating_host_url.host();
      if (initiator_host_match) {
        registry.AddThrottle(
            std::make_unique<GlicNavigationThrottle>(registry));
      }
    }
  }
}

GlicNavigationThrottle::GlicNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

GlicNavigationThrottle::~GlicNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GlicNavigationThrottle::WillStartRequest() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return CANCEL;
  }

  GURL url = navigation_handle()->GetURL();
  std::optional<std::string> cid;
  std::optional<std::string> continue_url_str;

  size_t max_cid_length = features::kGlicWebContinuityMaxCIDLength.Get();
  size_t max_target_url_length =
      features::kGlicWebContinuityMaxTargetUrlLength.Get();

  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == "cid") {
      std::string unescaped_cid = it.GetUnescapedValue();
      if (unescaped_cid.length() > max_cid_length) {
        return CANCEL;
      }
      cid = unescaped_cid;
    } else if (it.GetKey() == "targetUrl") {
      continue_url_str = it.GetUnescapedValue();
      if (continue_url_str->length() > max_target_url_length) {
        return CANCEL;
      }
    }
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  GlicKeyedService* glic_service = GlicKeyedService::Get(profile);
  if (!glic_service) {
    return PROCEED;
  }

  if (continue_url_str) {
    GURL continue_url(*continue_url_str);
    // TODO (b/484408637): Add support for non-HTTPS schemes.
    CHECK(continue_url.is_valid() && continue_url.SchemeIs(url::kHttpsScheme));
    if (continue_url.is_valid() && continue_url.SchemeIs(url::kHttpsScheme)) {
      tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(web_contents);
      if (tab && tab->GetBrowserWindowInterface()) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        glic_service->ShowUiWithConversationID(
            tab->GetBrowserWindowInterface(),
            glic::mojom::InvocationSource::kNavigationCapture, *cid);
#pragma clang diagnostic pop
      }

      NavigateParams params(profile, continue_url,
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
      params.disposition = WindowOpenDisposition::CURRENT_TAB;
      params.source_contents = web_contents;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce([](NavigateParams params) { Navigate(&params); },
                         std::move(params)));
    }
  }

  return CANCEL;
}

const char* GlicNavigationThrottle::GetNameForLogging() {
  return "GlicNavigationThrottle";
}

}  // namespace glic
