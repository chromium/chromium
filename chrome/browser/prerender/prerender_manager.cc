// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include "chrome/browser/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/page.h"

namespace {

std::u16string ExtractSearchTermsFromURL(content::WebContents& web_contents,
                                         const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents.GetBrowserContext());
  if (!profile)
    return u"";
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  // Can be nullptr in unit tests.
  if (!template_url_service)
    return u"";
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

}  // namespace

PrerenderManager::~PrerenderManager() = default;

void PrerenderManager::PrimaryPageChanged(content::Page& page) {
  search_prerender_handle_.reset();
  direct_url_input_prerender_handle_.reset();
}

base::WeakPtr<content::PrerenderHandle>
PrerenderManager::StartPrerenderDirectUrlInput(const GURL& prerendering_url) {
  if (direct_url_input_prerender_handle_ &&
      direct_url_input_prerender_handle_->GetInitialPrerenderingUrl() ==
          prerendering_url) {
    return nullptr;
  }
  direct_url_input_prerender_handle_.reset();
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

void PrerenderManager::CancelPrerenderDirectUrlInput() {
  direct_url_input_prerender_handle_.reset();
}

void PrerenderManager::StartPrerenderAutocompleteMatch(
    const AutocompleteMatch& match) {
  DCHECK(AutocompleteMatch::IsSearchType(match.type));
  TemplateURLRef::SearchTermsArgs& search_terms_args =
      *(match.search_terms_args);
  std::u16string search_terms = search_terms_args.search_terms;

  // Do not re-prerender the same search result.
  if (search_prerender_handle_ && prerendered_search_terms_ == search_terms) {
    return;
  }
  search_prerender_handle_.reset();
  prerendered_search_terms_ = search_terms;
  base::RepeatingCallback<bool(const GURL&)> url_match_predicate =
      base::BindRepeating(&IsSearchDestinationMatch,
                          std::ref(prerendered_search_terms_),
                          std::ref(*web_contents()));

  // TODO(https://crbug.com/1278634): Make up a destination url based on
  // DefaultSearchProvider. This can differ from `match.destination_url`.
  search_prerender_handle_ = web_contents()->StartPrerendering(
      match.destination_url, content::PrerenderTriggerType::kEmbedder,
      prerender_utils::kDefaultSearchEngineMetricSuffix,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      std::move(url_match_predicate));
}

PrerenderManager::PrerenderManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PrerenderManager>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrerenderManager);
