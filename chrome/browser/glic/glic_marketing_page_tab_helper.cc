// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_marketing_page_tab_helper.h"

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace glic {
namespace {

void RecordMarketingAutoOpen(Profile* profile) {
  if (profile) {
    PrefService* prefs = profile->GetPrefs();
    int current = prefs->GetInteger(prefs::kGlicMarketingAutoOpenCount);
    prefs->SetInteger(prefs::kGlicMarketingAutoOpenCount, current + 1);
  }
}

}  // namespace

GlicMarketingPageTabHelper::GlicMarketingPageTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<GlicMarketingPageTabHelper>(*web_contents) {}

GlicMarketingPageTabHelper::~GlicMarketingPageTabHelper() = default;

void GlicMarketingPageTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kGlicMarketingAutoOpen)) {
    return;
  }

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

  std::string allowlist_str = features::kGlicMarketingUrlAllowlist.Get();
  if (allowlist_str.empty()) {
    return;
  }

  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();

  // TODO(b/549556810): Cache the split strings instead of parsing every
  // time.
  std::vector<std::string_view> urls = base::SplitStringPiece(
      allowlist_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  bool is_matched = false;
  const GURL current_url_match =
      navigation_handle->GetURL().ReplaceComponents(replacements);

  for (const auto& url_str : urls) {
    GURL allowed_url(url_str);
    if (allowed_url.is_valid() && current_url_match == allowed_url) {
      is_matched = true;
      break;
    }
  }

  if (!is_matched) {
    return;
  }

  PrefService* pref_service = profile->GetPrefs();

  int current_count =
      pref_service->GetInteger(prefs::kGlicMarketingAutoOpenCount);
  int max_count = features::kGlicMarketingAutoOpenMaxCount.Get();

  if (current_count >= max_count) {
    return;
  }

  GlicKeyedService* glic_service = GlicKeyedService::Get(profile);
  if (glic_service) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents);

    if (!tab) {
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlicMarketingPageTabHelper);

}  // namespace glic
