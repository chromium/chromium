// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include <memory>
#include <string>

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/prerender_handle.h"
#include "content/public/browser/replaced_navigation_entry_data.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

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

std::u16string ExtractSearchTermsFromURL(
    const TemplateURLService* const template_url_service,
    const GURL& url) {
  // Can be nullptr in unit tests.
  if (!template_url_service) {
    return std::u16string();
  }
  auto* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_search_provider);
  std::u16string matched_search_terms;
  default_search_provider->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &matched_search_terms);
  return matched_search_terms;
}

std::u16string ExtractSearchTermsFromURL(content::WebContents& web_contents,
                                         const GURL& url) {
  const TemplateURLService* const template_url_service =
      GetTemplateURLServiceFromWebContents(web_contents);
  return ExtractSearchTermsFromURL(template_url_service, url);
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

// Prerendered pages are considered stale after a fixed duration.
// TODO(https://crbug.com/1295170): Use the search prefetch setting for now. The
// timedelta should be calculated by SearchPrefetchService after search
// prerender reuses the prefetched responses.
base::TimeDelta GetSearchPrerenderExpiryDuration() {
  return SearchPrefetchCachingLimit();
}

// TODO(https://crbug.com/1295170): This is a workaround. Remove this method
// after the unification work is done.
GURL RemoveParameterFromUrl(const GURL& url) {
  std::string query = url.query();
  base::ReplaceFirstSubstringAfterOffset(&query, /*start_offset=*/0, "&pf=cs",
                                         "");
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

}  // namespace

PrerenderManager::~PrerenderManager() = default;

class PrerenderManager::SearchPrerenderTask {
 public:
  SearchPrerenderTask(
      const std::u16string& search_terms,
      std::unique_ptr<content::PrerenderHandle> search_prerender_handle)
      : search_prerender_handle_(std::move(search_prerender_handle)),
        prerendered_search_terms_(search_terms) {
    expiry_timer_.Start(FROM_HERE, GetSearchPrerenderExpiryDuration(),
                        base::BindOnce(&SearchPrerenderTask::OnTimerTriggered,
                                       base::Unretained(this)));
  }

  ~SearchPrerenderTask() = default;

  // Not copyable or movable.
  SearchPrerenderTask(const SearchPrerenderTask&) = delete;
  SearchPrerenderTask& operator=(const SearchPrerenderTask&) = delete;

  const std::u16string& prerendered_search_terms() const {
    return prerendered_search_terms_;
  }

  void MaybeAppendUrlEntry(content::WebContents& web_contents) const {
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

    // TODO(https://crbug.com/1295170): This rule is hard coded according to
    // TemplateUrl, which is not good, and can be removed after the unification
    // work is done.
    const std::string prerender_key = "pf";

    // Maybe the prerendering page has updated its URL. In this case, obtain the
    // original URL with the ReplacedNavigationEntryData. The reason why we do
    // not compare the URL with GetInitialPrerenderingUrl here is that the URL
    // can be changed by other mechanisms, such as safe search.
    if (const absl::optional<content::ReplacedNavigationEntryData>&
            replaced_data = entry->GetReplacedEntryData()) {
      const GURL& maybe_prerendering_url = replaced_data->first_committed_url;
      std::string out_value;
      bool key_exists = net::GetValueForKeyInQuery(maybe_prerendering_url,
                                                   prerender_key, &out_value);
      if (key_exists &&
          !net::GetValueForKeyInQuery(web_contents.GetLastCommittedURL(),
                                      prerender_key, &out_value)) {
        search_prefetch_service->AddCacheEntryForPrerender(
            web_contents.GetLastCommittedURL(),
            replaced_data->first_committed_url);
        return;
      }
    }

    const GURL& activated_url = web_contents.GetLastCommittedURL();
    std::string out_value;
    bool key_exists =
        net::GetValueForKeyInQuery(activated_url, prerender_key, &out_value);
    if (key_exists) {
      GURL new_url = RemoveParameterFromUrl(activated_url);
      search_prefetch_service->AddCacheEntryForPrerender(new_url,
                                                         activated_url);
    }
  }

  void RecordTimestampOnDidStartNavigation(
      base::TimeTicks start_navigation_timestamp) {
    lastest_start_navigation_event_timestamp_ = start_navigation_timestamp;
  }

  void RecordLifeTimeMetric() {
    // Record the lifetime of this prerender.
    // |<------------GetSearchPrerenderExpiryDuration()------------>|
    // @ PrerenderHintReceived    @ Activation/NavigationStarted    @ Expire
    // |<---------delta---------->|
    // where:
    // expiry_timer_.desired_run_time() = Timestamp@Expire.
    // lastest_start_navigation_event_timestamp_ =
    //   Timestamp@Activation/NavigationStarted
    base::TimeDelta delta =
        GetSearchPrerenderExpiryDuration() -
        std::max(base::TimeDelta(),
                 expiry_timer_.desired_run_time() -
                     lastest_start_navigation_event_timestamp_);
    // The upper-bound of this histogram is decided by the default duration of
    // the search prefetch setting. See `prefetch_caching_limit_ms`.
    // TODO(https://crbug.com/1278634): Reconsider the duration after
    // PrerenderManager supports to re-prerender the search results.
    base::UmaHistogramCustomTimes(
        "Prerender.Experimental.Search."
        "FirstCorrectPrerenderHintReceivedToRealSearchNavigationStartedDuratio"
        "n",
        delta, base::Milliseconds(1), base::Seconds(60), /*buckets=*/50);
  }

 private:
  // Called by OneShotTimer. Will cancel the ongoing prerender to ensure the
  // content displayed to users is up-to-date.
  void OnTimerTriggered() { search_prerender_handle_.reset(); }

  std::unique_ptr<content::PrerenderHandle> search_prerender_handle_;

  // Recorded on OnDidStartNavigation and used on PrimaryPageChanged. Only the
  // latest recorded TimeTicks is meaningful. See the comment in
  // PrerenderManager::DidStartNavigation for more information.
  base::TimeTicks lastest_start_navigation_event_timestamp_;

  // Stops the ongoing prerender when the prerendered result is out-of-date.
  base::OneShotTimer expiry_timer_;

  // Stores the search term that `search_prerender_handle_` is prerendering.
  const std::u16string prerendered_search_terms_;
};

void PrerenderManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only watching the changes to primary main frame.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  // Ideally it should record the lifetime metric directly here if the search
  // terms match. However, the DidStartNavigation method can be called in other
  // cases(for example, the primary page has an ongoing navigation), and we only
  // care about the latest DidStartNavigation event right before
  // PrimaryPageChanged, and record metric if the search terms match(Note: we do
  // not only record the metric on the successful prerender activation, but also
  // on the failed cases, as long as the predictions are correct, since this
  // metric is used to understand the search prerender prediction rather than
  // the prerender operation). Besides this, it would waste the resources if we
  // parsed the URL for many times. i.e., in this method and in
  // PrimaryPageChanged. So it only records the timestamp, and
  // PrimaryPageChanged will record the metric later if needed.
  // TODO(https://crbug.com/1278634): Record the metrics at the moment
  // when a suggestion is selected.
  if (search_prerender_task_) {
    search_prerender_task_->RecordTimestampOnDidStartNavigation(
        navigation_handle->NavigationStart());
  }
}

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
  if (search_prerender_task_) {
    // TODO(https://crbug.com/1278634): re-prerender the search result if the
    // prerendered content has been removed.
    if (search_prerender_task_->prerendered_search_terms() == search_terms) {
      return;
    }

    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
        PrerenderPredictionStatus::kCancelled);
    search_prerender_task_.reset();
  }

  // web_contents() owns the instance that stores this callback, so it is safe
  // to call std::ref.
  base::RepeatingCallback<bool(const GURL&)> url_match_predicate =
      base::BindRepeating(&IsSearchDestinationMatch,
                          search_terms_args.search_terms,
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

    {
      // Undo the change. This information might be used during activation so
      // we should not change it.
      base::AutoReset<bool> resetter(&search_terms_args.is_prefetch, true);
      prerender_url = GURL(
          template_url_service->GetDefaultSearchProvider()
              ->url_ref()
              .ReplaceSearchTerms(search_terms_args,
                                  template_url_service->search_terms_data(),
                                  /*post_content=*/nullptr));
    }
    DCHECK(!search_terms_args.is_prefetch);
  }
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      web_contents()->StartPrerendering(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDefaultSearchEngineMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          std::move(url_match_predicate));

  if (prerender_handle) {
    search_prerender_task_ = std::make_unique<SearchPrerenderTask>(
        search_terms, std::move(prerender_handle));
  }
}

bool PrerenderManager::HasSearchResultPagePrerendered() const {
  return !!search_prerender_task_;
}

const std::u16string PrerenderManager::GetPrerenderSearchTermForTesting()
    const {
  return search_prerender_task_
             ? search_prerender_task_->prerendered_search_terms()
             : std::u16string();
}

void PrerenderManager::ResetPrerenderHandlesOnPrimaryPageChanged(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle->HasCommitted() &&
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
    direct_url_input_prerender_handle_.reset();
  }

  if (search_prerender_task_) {
    // TODO(https://crbug.com/1278634): Move all operations below into a
    // dedicated method of SearchPrerenderTask.

    // Record whether or not the prediction is correct when prerendering for
    // search suggestion was started. The value `kNotStarted` is recorded in
    // AutocompleteControllerAndroid::OnSuggestionSelected() or
    // ChromeOmniboxClient::OnURLOpenedFromOmnibox().
    bool is_search_destination_match = IsSearchDestinationMatch(
        search_prerender_task_->prerendered_search_terms(), *web_contents(),
        opened_url);
    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
        is_search_destination_match ? PrerenderPredictionStatus::kHitFinished
                                    : PrerenderPredictionStatus::kUnused);
    if (is_search_destination_match) {
      // We may want to record this metric on AutocompleteMatch selected relying
      // on GetMatchSelectionTimestamp. But this is for rough estimation so it
      // may not need the precise data.
      search_prerender_task_->RecordLifeTimeMetric();
    }

    if (prerender_utils::ShouldUpdateCacheEntryManually() &&
        is_search_destination_match &&
        navigation_handle->IsPrerenderedPageActivation()) {
      search_prerender_task_->MaybeAppendUrlEntry(*web_contents());
    }

    search_prerender_task_.reset();
  }
}

PrerenderManager::PrerenderManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PrerenderManager>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrerenderManager);
