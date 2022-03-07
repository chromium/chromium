// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"

namespace internal {
const char kHistogramPrerenderPredictionStatusDefaultSearchEngine[] =
    "Prerender.Experimental.PredictionStatus.DefaultSearchEngine";
const char kHistogramPrerenderPredictionStatusDirectUrlInput[] =
    "Prerender.Experimental.PredictionStatus.DirectUrlInput";
}  // namespace internal

namespace {

bool IsJavascriptDisabled(content::WebContents& web_contents, const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents.GetBrowserContext());
  if (!profile) {
    return true;
  }

  if (!profile->GetPrefs() ||
      !profile->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    return true;
  }

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return (!content_settings || content_settings->GetContentSetting(
                                   url, url, ContentSettingsType::JAVASCRIPT) ==
                                   CONTENT_SETTING_BLOCK);
}

TemplateURLService* GetTemplateURLServiceFromWebContents(
    content::WebContents& web_contents) {
  if (Profile* profile =
          Profile::FromBrowserContext(web_contents.GetBrowserContext())) {
    return TemplateURLServiceFactory::GetForProfile(profile);
  }
  return nullptr;
}

std::u16string ExtractSearchTermsFromURL(content::WebContents& web_contents,
                                         const GURL& url) {
  TemplateURLService* template_url_service =
      GetTemplateURLServiceFromWebContents(web_contents);
  // Can be nullptr in unit tests.
  if (!template_url_service) {
    return u"";
  }
  auto* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_search_provider);
  std::u16string matched_search_terms;
  default_search_provider->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &matched_search_terms);
  return matched_search_terms;
}

// Returns true when the two given URLs are considered as navigating to the same
// search term.
bool IsSearchDestinationMatch(const std::u16string& prerendered_search_terms,
                              content::WebContents& web_contents,
                              const GURL& navigation_url) {
  DCHECK(!prerendered_search_terms.empty());
  std::u16string matched_search_terms =
      ExtractSearchTermsFromURL(web_contents, navigation_url);
  return matched_search_terms == prerendered_search_terms;
}

// TODO(https://crbug.com/1291147): This is a workaround to stop the location
// bar from displaying the prefetch flag. This should be removed after we ensure
// the prerendered documents update the page by theirselves.
void UpdateVirtualUrlIfNecessary(content::WebContents& web_contents,
                                 TemplateURLRef::SearchTermsArgs& search_args,
                                 const GURL& prerendered_url) {
  content::NavigationController& controller = web_contents.GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (!entry) {
    return;
  }
  TemplateURLService* template_url_service =
      GetTemplateURLServiceFromWebContents(web_contents);
  DCHECK(template_url_service);

  const GURL& displayed_url = entry->GetVirtualURL();
  if (displayed_url == prerendered_url) {
    search_args.is_prefetch = false;
    entry->SetVirtualURL(
        GURL(template_url_service->GetDefaultSearchProvider()
                 ->url_ref()
                 .ReplaceSearchTerms(search_args,
                                     template_url_service->search_terms_data(),
                                     /*post_content=*/nullptr)));
  }
}

}  // namespace

PrerenderManager::~PrerenderManager() = default;

// TODO(crbug.com/1300416): Consider the incompatibility of precision/recall
// between NSP and Prerender2.
void PrerenderManager::PrimaryPageChanged(content::Page& page) {
  const GURL& opened_url = page.GetMainDocument().GetLastCommittedURL();

  if (direct_url_input_prerender_handle_) {
    // Record whether or not the prediction is correct when prerendering for
    // direct url input was started. The value `kNotStarted` is recorded in
    // AutocompleteActionPredictor::OnOmniboxOpenedUrl().
    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPredictionStatusDirectUrlInput,
        direct_url_input_prerender_handle_->GetInitialPrerenderingUrl() ==
                opened_url
            ? PrerenderPredictionStatus::kHitFinished
            : PrerenderPredictionStatus::kUnused);
    direct_url_input_prerender_handle_.reset();
  }

  if (search_prerender_handle_) {
    // Record whether or not the prediction is correct when prerendering for
    // search suggestion was started. The value `kNotStarted` is recorded in
    // AutocompleteControllerAndroid::OnSuggestionSelected() or
    // ChromeOmniboxClient::OnURLOpenedFromOmnibox().
    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
        IsSearchDestinationMatch(prerendered_search_terms_args_.search_terms,
                                 *web_contents(), opened_url)
            ? PrerenderPredictionStatus::kHitFinished
            : PrerenderPredictionStatus::kUnused);

    // If `skip_template_url_service_for_testing_` is set for testing, no
    // TemplateUrlService will be provided for updating the URL, so it needs not
    // to update the URL.
    if (prerender_utils::ShouldUpdateVirtualUrlForSearchManually() &&
        !skip_template_url_service_for_testing_) {
      GURL search_prerendered_url =
          search_prerender_handle_->GetInitialPrerenderingUrl();
      UpdateVirtualUrlIfNecessary(*web_contents(),
                                  prerendered_search_terms_args_,
                                  search_prerendered_url);
    }

    search_prerender_handle_.reset();
    prerendered_search_terms_args_ = TemplateURLRef::SearchTermsArgs();
  }
}

base::WeakPtr<content::PrerenderHandle>
PrerenderManager::StartPrerenderDirectUrlInput(const GURL& prerendering_url) {
  if (direct_url_input_prerender_handle_) {
    if (direct_url_input_prerender_handle_->GetInitialPrerenderingUrl() ==
        prerendering_url) {
      return direct_url_input_prerender_handle_->GetWeakPtr();
    }

    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPredictionStatusDirectUrlInput,
        PrerenderPredictionStatus::kCancelled);
    direct_url_input_prerender_handle_.reset();
  }
  direct_url_input_prerender_handle_ = web_contents()->StartPrerendering(
      prerendering_url, content::PrerenderTriggerType::kEmbedder,
      prerender_utils::kDirectUrlInputMetricSuffix,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  if (direct_url_input_prerender_handle_) {
    return direct_url_input_prerender_handle_->GetWeakPtr();
  }
  return nullptr;
}

void PrerenderManager::StartPrerenderSearchSuggestion(
    const AutocompleteMatch& match) {
  DCHECK(AutocompleteMatch::IsSearchType(match.type));

  // Since search pages require Javascirpt to perform the basic prerender
  // loading logic, do not prerender a search result if Javascript is disabled.
  if (IsJavascriptDisabled(*web_contents(), match.destination_url)) {
    return;
  }

  TemplateURLRef::SearchTermsArgs& search_terms_args =
      *(match.search_terms_args);
  const std::u16string& search_terms = search_terms_args.search_terms;

  // Do not re-prerender the same search result.
  if (search_prerender_handle_) {
    if (prerendered_search_terms_args_.search_terms == search_terms) {
      return;
    }

    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
        PrerenderPredictionStatus::kCancelled);
    search_prerender_handle_.reset();
  }

  // Make a copy. Use a copy instead of a reference, since we may modify it, and
  // we do not want to modify the original one which might be used to activate a
  // page.
  prerendered_search_terms_args_ = search_terms_args;

  // When prerendered_search_terms_args_ is reset, search_prerender_handle_
  // should be reset as well, which leads to the destruction of the instance
  // that owns this callback. So the content stored in
  // prerender_search_terms_arg_ outlives the callback, so it is safe to use
  // std::ref.
  // web_contents() owns the instance that stores this callback, so it is safe
  // to call std::ref.
  base::RepeatingCallback<bool(const GURL&)> url_match_predicate =
      base::BindRepeating(&IsSearchDestinationMatch,
                          std::ref(prerendered_search_terms_args_.search_terms),
                          std::ref(*web_contents()));

  GURL prerender_url = match.destination_url;

  // Skip changing the prerender URL in tests as they may not have Profile or
  // TemplateURLServiceFactory. In that case, the callers of
  // StartPrerenderSearchSuggestion() should ensure the prerender URL is valid
  // instead.
  if (!skip_template_url_service_for_testing_) {
    TemplateURLService* template_url_service =
        GetTemplateURLServiceFromWebContents(*web_contents());
    if (!template_url_service) {
      return;
    }

    prerendered_search_terms_args_.is_prefetch = true;
    prerender_url =
        GURL(template_url_service->GetDefaultSearchProvider()
                 ->url_ref()
                 .ReplaceSearchTerms(prerendered_search_terms_args_,
                                     template_url_service->search_terms_data(),
                                     /*post_content=*/nullptr));
  }

  search_prerender_handle_ = web_contents()->StartPrerendering(
      prerender_url, content::PrerenderTriggerType::kEmbedder,
      prerender_utils::kDefaultSearchEngineMetricSuffix,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      std::move(url_match_predicate));
}

PrerenderManager::PrerenderManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PrerenderManager>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrerenderManager);
