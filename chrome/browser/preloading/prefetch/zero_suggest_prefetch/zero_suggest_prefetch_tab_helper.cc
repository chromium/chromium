// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/zero_suggest_prefetch/zero_suggest_prefetch_tab_helper.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

using OEP = metrics::OmniboxEventProto;

// Returns whether or not the given URL represents a New Tab Page (NTP).
bool IsNTP(const GURL& url) {
  return url == GURL(chrome::kChromeUINewTabPageURL);
}

// Returns whether or not the given URL represents a prefetch-eligible Web page.
bool IsEligibleWebPage(const GURL& url) {
  return BaseSearchProvider::CanSendPageURLInRequest(url);
}

// Returns whether or not the given URL is eligible for ZPS prefetching.
bool IsURLEligibleForZPSPrefetching(const GURL& url) {
  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetching) &&
      IsNTP(url)) {
    return true;
  } else if (base::FeatureList::IsEnabled(
                 omnibox::kZeroSuggestPrefetchingOnSRP) &&
             google_util::IsGoogleSearchUrl(url)) {
    return true;
  } else if (base::FeatureList::IsEnabled(
                 omnibox::kZeroSuggestPrefetchingOnWeb) &&
             IsEligibleWebPage(url)) {
    return true;
  }
  return false;
}

// Starts prefetching zero-prefix suggestions using the AutocompleteController
// instance owned by the omnibox with a dedicated page context.
void StartPrefetch(content::WebContents* web_contents, const GURL& page_url) {
  auto* omnibox_view = search::GetOmniboxView(web_contents);
  if (!omnibox_view) {
    return;
  }

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  OEP::PageClassification page_classification = OEP::INVALID_SPEC;
  if (IsNTP(page_url)) {
    page_classification = OEP::NTP_ZPS_PREFETCH;
  } else if (google_util::IsGoogleSearchUrl(page_url)) {
    page_classification = OEP::SRP_ZPS_PREFETCH;
  } else if (IsEligibleWebPage(page_url)) {
    page_classification = OEP::OTHER_ZPS_PREFETCH;
  }
  DCHECK(page_classification != OEP::INVALID_SPEC)
      << "Prefetch page classification undefined for given URL.";

  AutocompleteInput autocomplete_input(
      u"", page_classification, ChromeAutocompleteSchemeClassifier(profile));
  autocomplete_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);
  // Construct proper on-clobber input for ZPS prefetch requests on SRP/Web.
  if (page_classification != OEP::NTP_ZPS_PREFETCH) {
    autocomplete_input.set_focus_type(
        metrics::OmniboxFocusType::INTERACTION_CLOBBER);
    autocomplete_input.set_current_url(page_url);
  }
  omnibox_view->StartPrefetch(autocomplete_input);
}

}  // namespace

ZeroSuggestPrefetchTabHelper::ZeroSuggestPrefetchTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ZeroSuggestPrefetchTabHelper>(
          *web_contents) {}

ZeroSuggestPrefetchTabHelper::~ZeroSuggestPrefetchTabHelper() = default;

void ZeroSuggestPrefetchTabHelper::PrimaryPageChanged(content::Page& page) {
  const auto& last_committed_url = page.GetMainDocument().GetLastCommittedURL();
  if (!IsURLEligibleForZPSPrefetching(last_committed_url))
    return;

  // Make sure to observe the TabStripModel, if not already, in order to get
  // notified when a New Tab Page is switched to.
  // Note that this is done here, i.e., after the New Tab Page is navigated to,
  // as opposed to the tab helper constructor which would have allowed us to get
  // notified when a new tab is opened in the foreground in the same
  // TabStripModelObserver callback. We are however not interested to start
  // prefetching that early since the AutocompleteController machinery gets
  // started and stopped multiple times since a new tab is opened and until the
  // New Tab Page is navigated to; invalidating prefetch requests prematurely.
  auto* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser && !TabStripModelObserver::IsObservingAny(this)) {
    browser->tab_strip_model()->AddObserver(this);
  }

  StartPrefetch(web_contents(), last_committed_url);
}

void ZeroSuggestPrefetchTabHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  const auto& last_committed_url =
      web_contents()->GetPrimaryPage().GetMainDocument().GetLastCommittedURL();
  if (!selection.active_tab_changed() ||
      web_contents() != selection.new_contents ||
      !IsURLEligibleForZPSPrefetching(last_committed_url)) {
    return;
  }

  // We get here when a New Tab Page is brought to foreground (aka switched to).
  StartPrefetch(web_contents(), last_committed_url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ZeroSuggestPrefetchTabHelper);
