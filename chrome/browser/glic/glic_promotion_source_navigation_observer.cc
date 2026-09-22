// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_promotion_source_navigation_observer.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/glic/glic_metrics_provider.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

namespace glic {
namespace {

constexpr char kPromotionPagePathSuffix[] =
    "/chrome/ai-innovations/gemini-in-chrome";
constexpr char kUtmMediumParam[] = "utm_medium";
constexpr char kSourceParam[] = "source";
constexpr char kChromeWebStoreMedium[] = "chrome-web-store";
constexpr char kZssSource[] = "zss";

const GURL* g_promotion_page_url_for_testing = nullptr;

bool IsPromotionPageUrl(const GURL& url) {
  if (g_promotion_page_url_for_testing &&
      !g_promotion_page_url_for_testing->is_empty()) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    replacements.ClearRef();
    return url.ReplaceComponents(replacements) ==
           g_promotion_page_url_for_testing->ReplaceComponents(replacements);
  }
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      !url.DomainIs("google.com")) {
    return false;
  }
  std::string_view path = url.path();
  if (path.ends_with('/')) {
    path.remove_suffix(1);
  }
  return path.ends_with(kPromotionPagePathSuffix);
}

}  // namespace

GlicPromotionSourceNavigationObserver::GlicPromotionSourceNavigationObserver(
    tabs::TabInterface* tab)
    : content::WebContentsObserver(tab->GetContents()),
      tab_subscription_(tab->RegisterWillDiscardContents(base::BindRepeating(
          &GlicPromotionSourceNavigationObserver::WillDiscardContents,
          base::Unretained(this)))) {}

GlicPromotionSourceNavigationObserver::
    ~GlicPromotionSourceNavigationObserver() = default;

void GlicPromotionSourceNavigationObserver::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
}

// static
void GlicPromotionSourceNavigationObserver::SetPromotionPageUrlForTesting(
    const GURL& url) {
  static base::NoDestructor<GURL> test_url;
  *test_url = url;
  g_promotion_page_url_for_testing = url.is_empty() ? nullptr : test_url.get();
}

void GlicPromotionSourceNavigationObserver::MaybeRegisterPromotionSourceCohort(
    Profile* profile,
    content::NavigationHandle* navigation_handle) {
  if (!profile || !navigation_handle) {
    return;
  }

  PrefService* pref_service = profile->GetPrefs();
  if (!pref_service) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  std::string utm_medium;
  bool has_utm_medium =
      net::GetValueForKeyInQuery(url, kUtmMediumParam, &utm_medium);
  std::string source;
  bool has_source = net::GetValueForKeyInQuery(url, kSourceParam, &source);

  std::string new_group;
  if (utm_medium == kChromeWebStoreMedium) {
    new_group = kGlicPromotionSourceWebstore;
  } else if (source == kZssSource) {
    new_group = kGlicPromotionSourceZss;
  } else if (!has_utm_medium && !has_source) {
    new_group = kGlicPromotionSourceChromeDotCom;
  } else {
    return;
  }

  const std::string current_cohort =
      pref_service->GetString(prefs::kGlicPromotionSourceCohort);

  std::string final_group;
  if (current_cohort.empty()) {
    final_group = new_group;
  } else if (current_cohort == new_group ||
             current_cohort == kGlicPromotionSourceMultiple) {
    return;
  } else {
    final_group = kGlicPromotionSourceMultiple;
  }

  pref_service->SetString(prefs::kGlicPromotionSourceCohort, final_group);
  GlicMetricsProvider::RegisterPromotionSourceSyntheticTrial(profile);
}

void GlicPromotionSourceNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!GlicEnabling::IsEnabledForProfile(profile)) {
    return;
  }

  if (!IsPromotionPageUrl(navigation_handle->GetURL())) {
    return;
  }

  MaybeRegisterPromotionSourceCohort(profile, navigation_handle);
}

}  // namespace glic
