// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/prerender_handle.h"
#include "content/public/browser/replaced_navigation_entry_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace internal {
const char kHistogramPrerenderPredictionStatusDefaultSearchEngine[] =
    "Prerender.Experimental.PredictionStatus.DefaultSearchEngine";
const char kHistogramPrerenderPredictionStatusDirectUrlInput[] =
    "Prerender.Experimental.PredictionStatus.DirectUrlInput";
}  // namespace internal

namespace {

using content::PreloadingTriggeringOutcome;

void MarkPreloadingAttemptAsDuplicate(
    content::PreloadingAttempt* preloading_attempt) {
  CHECK(!preloading_attempt->ShouldHoldback());
  preloading_attempt->SetTriggeringOutcome(
      PreloadingTriggeringOutcome::kDuplicate);
}

content::PreloadingFailureReason ToPreloadingFailureReason(
    PrerenderPredictionStatus status) {
  return static_cast<content::PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(content::PreloadingFailureReason::
                           kPreloadingFailureReasonContentEnd));
}

void AttachBookmarkBarNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle, page_load_metrics::NavigationHandleUserData::
                             InitiatorLocation::kBookmarkBar);
}

bool IsSearchUrl(content::WebContents& web_contents, const GURL& url) {
  auto* profile = Profile::FromBrowserContext(web_contents.GetBrowserContext());
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  return template_url_service &&
         template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
             url);
}

}  // namespace

PrerenderManager::~PrerenderManager() = default;

class PrerenderManager::SearchPrerenderTask {
 public:
  SearchPrerenderTask(
      const GURL& canonical_search_url,
      std::unique_ptr<content::PrerenderHandle> search_prerender_handle)
      : search_prerender_handle_(std::move(search_prerender_handle)),
        prerendered_canonical_search_url_(canonical_search_url) {}

  ~SearchPrerenderTask() {
    // Record whether or not the prediction is correct when prerendering for
    // search suggestion was started. The value `kNotStarted` is recorded in
    // AutocompleteControllerAndroid::OnSuggestionSelected() or
    // ChromeOmniboxClient::OnURLOpenedFromOmnibox() if there is no started
    // prerender.
    CHECK_NE(prediction_status_, PrerenderPredictionStatus::kNotStarted);
    SetFailureReason(prediction_status_);
    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
        prediction_status_);
  }

  void SetFailureReason(PrerenderPredictionStatus status) {
    if (!search_prerender_handle_) {
      return;
    }
    switch (status) {
      case PrerenderPredictionStatus::kNotStarted:
      case PrerenderPredictionStatus::kCancelled:
        search_prerender_handle_->SetPreloadingAttemptFailureReason(
            ToPreloadingFailureReason(status));
        return;
      case PrerenderPredictionStatus::kUnused:
      case PrerenderPredictionStatus::kHitFinished:
        // Only set failure reasons for failing cases. kUnused and kHitFinished
        // are not considered prerender failures.
        return;
    }
  }

  // Not copyable or movable.
  SearchPrerenderTask(const SearchPrerenderTask&) = delete;
  SearchPrerenderTask& operator=(const SearchPrerenderTask&) = delete;

  const GURL& prerendered_canonical_search_url() const {
    return prerendered_canonical_search_url_;
  }

  void OnActivated(content::WebContents& web_contents) const {
    if (!search_prerender_handle_) {
      return;
    }
    content::NavigationController& controller = web_contents.GetController();
    content::NavigationEntry* entry = controller.GetVisibleEntry();
    if (!entry) {
      return;
    }
    SearchPrefetchService* search_prefetch_service =
        SearchPrefetchServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents.GetBrowserContext()));
    if (!search_prefetch_service) {
      return;
    }

    search_prefetch_service->OnPrerenderedRequestUsed(
        prerendered_canonical_search_url_, web_contents.GetLastCommittedURL());
  }

  void set_prediction_status(PrerenderPredictionStatus prediction_status) {
    // If the final status was set, do nothing because the status has been
    // finalized.
    if (prediction_status_ != PrerenderPredictionStatus::kUnused)
      return;
    CHECK_NE(prediction_status, PrerenderPredictionStatus::kUnused);
    prediction_status_ = prediction_status;
  }

 private:
  std::unique_ptr<content::PrerenderHandle> search_prerender_handle_;

  // A task is associated with a prediction, this tracks the correctness of the
  // prediction.
  PrerenderPredictionStatus prediction_status_ =
      PrerenderPredictionStatus::kUnused;

  // Stores the search term that `search_prerender_handle_` is prerendering.
  const GURL prerendered_canonical_search_url_;
};

void PrerenderManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // This is a primary page change. Reset the prerender handles.
  // PrerenderManager does not listen to the PrimaryPageChanged event, because
  // it needs the navigation_handle to figure out whether the PrimaryPageChanged
  // event is caused by prerender activation.
  ResetPrerenderHandlesOnPrimaryPageChanged(navigation_handle);
}

base::WeakPtr<content::PrerenderHandle>
PrerenderManager::StartPrerenderBookmark(const GURL& prerendering_url) {
  // Helpers to create content::PreloadingAttempt.
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(web_contents());
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(prerendering_url);

  if (IsSearchUrl(*web_contents(), prerendering_url)) {
    return nullptr;
  }

  // Create new PreloadingAttempt and pass all the values corresponding to
  // this prerendering attempt for Prerender.
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar,
          content::PreloadingType::kPrerender, std::move(same_url_matcher),
          /*planned_max_preloading_type=*/std::nullopt,
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  // BookmarkBar only allows https protocol.
  // TODO(crbug.com/40259793): Add an enum metric to report the protocol scheme
  // to decide if we should loosen this restriction for the http scheme.
  if (!prerendering_url.SchemeIs("https")) {
    preloading_attempt->SetEligibility(
        content::PreloadingEligibility::kHttpsOnly);
    return nullptr;
  }

  if (bookmark_prerender_handle_) {
    if (bookmark_prerender_handle_->GetInitialPrerenderingUrl() ==
        prerendering_url) {
      // In case a prerender is already present for the URL, prerendering is
      // eligible but mark triggering outcome as a duplicate.
      preloading_attempt->SetEligibility(
          content::PreloadingEligibility::kEligible);

      MarkPreloadingAttemptAsDuplicate(preloading_attempt);
      return bookmark_prerender_handle_->GetWeakPtr();
    }
    bookmark_prerender_handle_.reset();
  }

  base::RepeatingCallback<void(content::NavigationHandle&)>
      prerender_navigation_handle_callback =
          base::BindRepeating(&AttachBookmarkBarNavigationHandleUserData);

  bookmark_prerender_handle_ = web_contents()->StartPrerendering(
      prerendering_url, content::PreloadingTriggerType::kEmbedder,
      prerender_utils::kBookmarkBarMetricSuffix,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK),
      // Considering the characteristics of triggers (e.g., the duration from
      // trigger to activation), warm-up is not enabled for now on this trigger.
      // Please see crbug and its doc for more details.
      /*should_warm_up_compositor=*/false,
      content::PreloadingHoldbackStatus::kUnspecified, preloading_attempt,
      /*url_match_predicate=*/{},
      std::move(prerender_navigation_handle_callback));

  return bookmark_prerender_handle_ ? bookmark_prerender_handle_->GetWeakPtr()
                                    : nullptr;
}

base::WeakPtr<content::PrerenderHandle>
PrerenderManager::StartPrerenderNewTabPage(
    const GURL& prerendering_url,
    content::PreloadingPredictor predictor) {
  if (IsSearchUrl(*web_contents(), prerendering_url)) {
    return nullptr;
  }

  // Helpers to create content::PreloadingAttempt.
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(web_contents());
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(prerendering_url);

  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          predictor, content::PreloadingType::kPrerender,
          std::move(same_url_matcher),
          /*planned_max_preloading_type=*/std::nullopt,
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  // New Tab Page only allow https protocol.
  if (!prerendering_url.SchemeIs("https")) {
    preloading_attempt->SetEligibility(
        content::PreloadingEligibility::kHttpsOnly);
    return nullptr;
  }

  if (new_tab_page_prerender_handle_) {
    if (new_tab_page_prerender_handle_->GetInitialPrerenderingUrl() ==
        prerendering_url) {
      // In case a prerender is already present for the URL, prerendering is
      // eligible but mark triggering outcome as a duplicate.
      preloading_attempt->SetEligibility(
          content::PreloadingEligibility::kEligible);

      MarkPreloadingAttemptAsDuplicate(preloading_attempt);
      return new_tab_page_prerender_handle_->GetWeakPtr();
    }
    new_tab_page_prerender_handle_.reset();
  }

  base::RepeatingCallback<void(content::NavigationHandle&)>
      prerender_navigation_handle_callback =
          base::BindRepeating(&page_load_metrics::NavigationHandleUserData::
                                  AttachNewTabPageNavigationHandleUserData);

  new_tab_page_prerender_handle_ = web_contents()->StartPrerendering(
      prerendering_url, content::PreloadingTriggerType::kEmbedder,
      prerender_utils::kNewTabPageMetricSuffix,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK),
      // Considering the characteristics of triggers (e.g., the duration from
      // trigger to activation), warm-up is not enabled for now on this trigger.
      // Please see crbug and its doc for more details.
      /*should_warm_up_compositor=*/false,
      content::PreloadingHoldbackStatus::kUnspecified, preloading_attempt,
      /*url_match_predicate=*/{},
      std::move(prerender_navigation_handle_callback));

  return new_tab_page_prerender_handle_
             ? new_tab_page_prerender_handle_->GetWeakPtr()
             : nullptr;
}

void PrerenderManager::StopPrerenderNewTabPage(
    base::WeakPtr<content::PrerenderHandle> prerender_handle) {
  if (!prerender_handle) {
    return;
  }
  CHECK(new_tab_page_prerender_handle_);
  CHECK_EQ(prerender_handle.get(),
           new_tab_page_prerender_handle_->GetWeakPtr().get());
  new_tab_page_prerender_handle_.reset();
}

void PrerenderManager::StopPrerenderBookmark(
    base::WeakPtr<content::PrerenderHandle> prerender_handle) {
  if (!prerender_handle) {
    return;
  }
  CHECK_EQ(prerender_handle.get(),
           bookmark_prerender_handle_->GetWeakPtr().get());
  bookmark_prerender_handle_.reset();
}

base::WeakPtr<content::PrerenderHandle>
PrerenderManager::StartPrerenderDirectUrlInput(
    const GURL& prerendering_url,
    content::PreloadingAttempt& preloading_attempt) {
  if (direct_url_input_prerender_handle_) {
    if (direct_url_input_prerender_handle_->GetInitialPrerenderingUrl() ==
        prerendering_url) {
      // In case a prerender is already present for the URL, prerendering is
      // eligible but mark triggering outcome as a duplicate.
      preloading_attempt.SetEligibility(
          content::PreloadingEligibility::kEligible);

      MarkPreloadingAttemptAsDuplicate(&preloading_attempt);
      return direct_url_input_prerender_handle_->GetWeakPtr();
    }

    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPredictionStatusDirectUrlInput,
        PrerenderPredictionStatus::kCancelled);
    // Mark the previous prerender as failure as we can't keep multiple DUI
    // prerenders active at the same time.
    direct_url_input_prerender_handle_->SetPreloadingAttemptFailureReason(
        ToPreloadingFailureReason(PrerenderPredictionStatus::kCancelled));
    direct_url_input_prerender_handle_.reset();
  }
  direct_url_input_prerender_handle_ = web_contents()->StartPrerendering(
      prerendering_url, content::PreloadingTriggerType::kEmbedder,
      prerender_utils::kDirectUrlInputMetricSuffix,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*should_warm_up_compositor=*/true,
      content::PreloadingHoldbackStatus::kUnspecified, &preloading_attempt,
      /*url_match_predicate=*/{}, /*prerender_navigation_handle_callback=*/{});

  if (direct_url_input_prerender_handle_) {
    return direct_url_input_prerender_handle_->GetWeakPtr();
  }
  return nullptr;
}

void PrerenderManager::StartPrerenderSearchResult(
    const GURL& canonical_search_url,
    const GURL& prerendering_url,
    base::WeakPtr<content::PreloadingAttempt> preloading_attempt) {
  // If the caller does not want to prerender a new result, this does not need
  // to do anything.
  if (!ResetSearchPrerenderTaskIfNecessary(canonical_search_url,
                                           preloading_attempt)) {
    return;
  }

  // web_contents() owns the instance that stores this callback, so it is safe
  // to call std::ref.
  base::RepeatingCallback<bool(const GURL&,
                               const std::optional<content::UrlMatchType>&)>
      url_match_predicate = base::BindRepeating(
          &IsSearchDestinationMatchWithWebUrlMatchResult, canonical_search_url,
          web_contents()->GetBrowserContext());

  content::PreloadingHoldbackStatus holdback_status_override =
      content::PreloadingHoldbackStatus::kUnspecified;
  if (base::FeatureList::IsEnabled(features::kPrerenderDSEHoldback)) {
    holdback_status_override = content::PreloadingHoldbackStatus::kHoldback;
  }

  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      web_contents()->StartPrerendering(
          prerendering_url, content::PreloadingTriggerType::kEmbedder,
          prerender_utils::kDefaultSearchEngineMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*should_warm_up_compositor=*/true, holdback_status_override,
          preloading_attempt.get(), std::move(url_match_predicate),
          /*prerender_navigation_handle_callback=*/{});

  if (prerender_handle) {
    CHECK(!search_prerender_task_)
        << "SearchPrerenderTask should be reset before setting a new one.";
    search_prerender_task_ = std::make_unique<SearchPrerenderTask>(
        canonical_search_url, std::move(prerender_handle));
  }
}

void PrerenderManager::StopPrerenderSearchResult(
    const GURL& canonical_search_url) {
  if (search_prerender_task_ &&
      search_prerender_task_->prerendered_canonical_search_url() ==
          canonical_search_url) {
    // TODO(crbug.com/40214220): Now there is no kUnused record: all the
    // unused tasks are canceled before navigation happens. Consider recording
    // the result upon opening the URL rather than waiting for the navigation
    // finishes.
    search_prerender_task_->set_prediction_status(
        PrerenderPredictionStatus::kCancelled);
    search_prerender_task_.reset();
  }
}

bool PrerenderManager::HasSearchResultPagePrerendered() const {
  return !!search_prerender_task_;
}

base::WeakPtr<PrerenderManager> PrerenderManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const GURL PrerenderManager::GetPrerenderCanonicalSearchURLForTesting() const {
  return search_prerender_task_
             ? search_prerender_task_->prerendered_canonical_search_url()
             : GURL();
}

void PrerenderManager::ResetPrerenderHandlesOnPrimaryPageChanged(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle->HasCommitted() &&
        navigation_handle->IsInPrimaryMainFrame() &&
        !navigation_handle->IsSameDocument());
  const GURL& opened_url = navigation_handle->GetURL();
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
    // We don't set the PreloadingFailureReason for wrong predictions, as this
    // is not a prerender failure rather it is an in accurate triggering for DUI
    // predictor as the user didn't end up navigating to the predicted URL.
    direct_url_input_prerender_handle_.reset();
  }

  if (search_prerender_task_) {
    // TODO(crbug.com/40208255): Move all operations below into a
    // dedicated method of SearchPrerenderTask.

    bool is_search_destination_match = IsSearchDestinationMatch(
        search_prerender_task_->prerendered_canonical_search_url(),
        web_contents()->GetBrowserContext(), opened_url);

    if (is_search_destination_match) {
      search_prerender_task_->set_prediction_status(
          PrerenderPredictionStatus::kHitFinished);
    }

    if (is_search_destination_match &&
        navigation_handle->IsPrerenderedPageActivation()) {
      search_prerender_task_->OnActivated(*web_contents());
    }

    search_prerender_task_.reset();
  }

  bookmark_prerender_handle_.reset();
  new_tab_page_prerender_handle_.reset();
}

bool PrerenderManager::ResetSearchPrerenderTaskIfNecessary(
    const GURL& canonical_search_url,
    base::WeakPtr<content::PreloadingAttempt> preloading_attempt) {
  if (!search_prerender_task_)
    return true;

  // Do not re-prerender the same search result.
  if (search_prerender_task_->prerendered_canonical_search_url() ==
      canonical_search_url) {
    // In case a prerender is already present for the URL, prerendering is
    // eligible but mark triggering outcome as a duplicate.
    if (preloading_attempt) {
      preloading_attempt->SetEligibility(
          content::PreloadingEligibility::kEligible);

      MarkPreloadingAttemptAsDuplicate(preloading_attempt.get());
    }
    return false;
  }
  search_prerender_task_->set_prediction_status(
      PrerenderPredictionStatus::kCancelled);
  search_prerender_task_.reset();
  return true;
}

PrerenderManager::PrerenderManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PrerenderManager>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrerenderManager);
