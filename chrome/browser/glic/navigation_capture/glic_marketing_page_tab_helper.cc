// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/navigation_capture/glic_marketing_page_tab_helper.h"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace glic {
namespace {

std::vector<GURL> ParseAllowedMarketingUrls(std::string_view allowlist_str) {
  std::vector<GURL> urls;
  for (std::string_view piece :
       base::SplitStringPiece(allowlist_str, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    GURL url(piece);
    if (url.is_valid()) {
      urls.push_back(std::move(url));
    }
  }
  return urls;
}

void RecordMarketingAutoOpen(Profile* profile) {
  if (profile) {
    PrefService* prefs = profile->GetPrefs();
    int current = prefs->GetInteger(prefs::kGlicMarketingAutoOpenCount);
    prefs->SetInteger(prefs::kGlicMarketingAutoOpenCount, current + 1);
  }
}

}  // namespace

// static
std::unique_ptr<GlicMarketingPageTabHelper>
GlicMarketingPageTabHelper::MaybeCreate(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const GlicProviderSpec& spec = GetGlicProviderSpec(profile);
  if (!spec.SupportsFeature(GlicFeature::kMarketingAutoOpen)) {
    return nullptr;
  }
  const GlicMarketingAutoOpenFeatureParams params =
      spec.GetMarketingAutoOpenFeatureParams();
  std::vector<GURL> allowed_urls =
      ParseAllowedMarketingUrls(params.allowlisted_urls);
  if (allowed_urls.empty()) {
    return nullptr;
  }
  return base::WrapUnique(new GlicMarketingPageTabHelper(
      web_contents, std::move(allowed_urls), params.max_impressions));
}

GlicMarketingPageTabHelper::GlicMarketingPageTabHelper(
    content::WebContents* web_contents,
    std::vector<GURL> allowed_urls,
    int max_impressions)
    : content::WebContentsObserver(web_contents),
      allowed_urls_(std::move(allowed_urls)),
      max_impressions_(max_impressions) {}

GlicMarketingPageTabHelper::~GlicMarketingPageTabHelper() = default;

void GlicMarketingPageTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

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

  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();

  const GURL current_url_match =
      navigation_handle->GetURL().ReplaceComponents(replacements);

  if (!std::ranges::contains(allowed_urls_, current_url_match)) {
    return;
  }

  PrefService* pref_service = profile->GetPrefs();

  int current_count =
      pref_service->GetInteger(prefs::kGlicMarketingAutoOpenCount);
  if (current_count >= max_impressions_) {
    return;
  }

  GlicKeyedService* glic_service = GlicKeyedService::Get(profile);
  if (glic_service) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents);

    if (!tab || glic_service->GetInstanceForTab(tab)) {
      return;
    }

    glic::Target target(*tab, glic::NewConversation());

    GlicInvokeOptions options(std::move(target),
                              glic::mojom::InvocationSource::kPromotionPage);
    options.fre_override = glic::mojom::FreOverride::kTrustFirstInline;
    options.fre_completion_wait_mode = FreCompletionWaitMode::kDefault;
    options.feature_mode = glic::mojom::FeatureMode::kPromotionPage;

    auto instance = glic_service->Invoke(std::move(options));

    if (instance) {
      RecordMarketingAutoOpen(profile);
    }
  }
}

}  // namespace glic
