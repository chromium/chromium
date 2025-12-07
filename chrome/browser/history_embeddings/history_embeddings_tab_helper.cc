// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"

#include <numeric>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"
#include "url/gurl.h"

namespace {

// This corresponds to UMA histogram enum `EmbeddingsExtractionCancelled`
// in tools/metrics/histograms/metadata/history/enums.xml
enum class ExtractionCancelled {
  UNKNOWN = 0,
  TAB_HELPER_DID_FINISH_LOAD = 1,
  TAB_HELPER_EXTRACT_PASSAGES_URL = 2,
  TAB_HELPER_EXTRACT_PASSAGES_RESCHEDULE = 3,
  TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_RESULTS = 4,
  TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_TIME = 5,
  TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_GUID = 6,
  SERVICE_RETRIEVE_PASSAGES = 7,
  DEPRECATED_SERVICE_RETRIEVE_PASSAGES_WITH_URL_DATA = 8,

  // These enum values are logged in UMA. Do not reuse or skip any values.
  // The order doesn't need to be chronological, but keep identities stable.
  ENUM_COUNT,
};

// Record UMA histogram with cancellation reason when extraction,
// embedding, etc. is cancelled before completion and storage.
void RecordExtractionCancelled(ExtractionCancelled reason) {
  base::UmaHistogramEnumeration("History.Embeddings.ExtractionCancelled",
                                reason, ExtractionCancelled::ENUM_COUNT);
}

void OnGotInnerText(mojo::Remote<blink::mojom::InnerTextAgent> remote,
                    std::string title,
                    base::ElapsedTimer passage_extraction_timer,
                    base::OnceCallback<void(std::vector<std::string>)> callback,
                    blink::mojom::InnerTextFramePtr mojo_frame) {
  std::vector<std::string> valid_passages;
  if (mojo_frame) {
    for (const auto& segment : mojo_frame->segments) {
      if (segment->is_text()) {
        valid_passages.emplace_back(segment->get_text());
      }
    }
    base::UmaHistogramTimes("History.Embeddings.Passages.ExtractionTime",
                            passage_extraction_timer.Elapsed());
  }
  const size_t total_text_size =
      std::accumulate(valid_passages.cbegin(), valid_passages.cend(), 0u,
                      [](size_t acc, const std::string& passage) {
                        return acc + passage.size();
                      });
  base::UmaHistogramCounts1000("History.Embeddings.Passages.PassageCount",
                               valid_passages.size());
  base::UmaHistogramCounts10M("History.Embeddings.Passages.TotalTextSize",
                              total_text_size);

  bool title_inserted = false;
  if (history_embeddings::GetFeatureParameters().insert_title_passage &&
      !title.empty() && !base::Contains(valid_passages, title)) {
    VLOG(2) << "Title passage inserted: " << title;
    valid_passages.insert(valid_passages.begin(), std::move(title));
    if (valid_passages.size() >
        static_cast<size_t>(
            history_embeddings::GetFeatureParameters().max_passages_per_page)) {
      valid_passages.pop_back();
    }
    title_inserted = true;
  }
  base::UmaHistogramBoolean("History.Embeddings.Passages.TitleInserted",
                            title_inserted);

  std::move(callback).Run(std::move(valid_passages));
}

}  // namespace

HistoryEmbeddingsTabHelper::HistoryEmbeddingsTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<HistoryEmbeddingsTabHelper>(*web_contents) {
  resource_coordinator::TabLoadTracker::Get()->AddObserver(this);
}

HistoryEmbeddingsTabHelper::~HistoryEmbeddingsTabHelper() {
  resource_coordinator::TabLoadTracker::Get()->RemoveObserver(this);
}

void HistoryEmbeddingsTabHelper::OnUpdatedHistoryForNavigation(
    content::NavigationHandle* navigation_handle,
    base::Time visit_time,
    const GURL& url) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !history_embeddings::IsHistoryEmbeddingsEnabledForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext())) ||
      !GetHistoryEmbeddingsService() || !GetPassageEmbedderModelObserver()) {
    return;
  }

  // Invalidate existing weak pointers to cancel any outstanding delayed tasks.
  // Since this is a new navigation, the document may have changed and so
  // the visit time and URL data would not match the web content.
  CancelExtraction();

  // Save data for later use in `DidFinishLoad`.
  history_visit_time_ = visit_time;
  history_url_ = url;
}

void HistoryEmbeddingsTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame() ||
      !history_embeddings::IsHistoryEmbeddingsEnabledForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext())) ||
      !GetHistoryEmbeddingsService() ||
      !GetHistoryEmbeddingsService()->IsEligible(validated_url)) {
    return;
  }

  // Invalidate existing weak pointers to cancel any outstanding delayed tasks.
  // Normally, navigation will have already canceled, but in the event that
  // `DidFinishLoad` is called twice without navigation, invalidating here
  // guarantees at most one delayed task is scheduled at a time.
  CancelExtraction();

  VLOG(3) << "Tabs Loading: "
          << resource_coordinator::TabLoadTracker::Get()->GetLoadingTabCount();

  if (!ScheduleExtraction(render_frame_host->GetWeakDocumentPtr())) {
    RecordExtractionCancelled(ExtractionCancelled::TAB_HELPER_DID_FINISH_LOAD);
    VLOG(3) << "Extraction cancelled in DidFinishLoad";
  }
}

void HistoryEmbeddingsTabHelper::OnLoadingStateChange(
    content::WebContents* web_contents,
    LoadingState old_loading_state,
    LoadingState new_loading_state) {
  size_t loading_tab_count =
      resource_coordinator::TabLoadTracker::Get()->GetLoadingTabCount();
  VLOG(3) << "Loading state changed for '" << web_contents->GetTitle()
          << "' with " << loading_tab_count << " tabs now loading.";
}

void HistoryEmbeddingsTabHelper::RetrievePassagesForTesting(
    history::URLID url_id,
    history::VisitID visit_id,
    base::Time visit_time,
    content::WeakDocumentPtr weak_render_frame_host) {
  RetrievePassages(url_id, visit_id, visit_time, weak_render_frame_host);
}

bool HistoryEmbeddingsTabHelper::ScheduleExtraction(
    content::WeakDocumentPtr weak_render_frame_host) {
  if (!weak_render_frame_host.AsRenderFrameHostIfValid()) {
    return false;
  }
  // Schedule a new delayed task with a fresh weak pointer.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HistoryEmbeddingsTabHelper::ExtractPassages,
                     weak_ptr_factory_.GetWeakPtr(), weak_render_frame_host),
      base::Milliseconds(
          history_embeddings::GetFeatureParameters().passage_extraction_delay));
  return true;
}

void HistoryEmbeddingsTabHelper::ExtractPassages(
    content::WeakDocumentPtr weak_render_frame_host) {
  if (!history_url_.has_value()) {
    RecordExtractionCancelled(
        ExtractionCancelled::TAB_HELPER_EXTRACT_PASSAGES_URL);
    VLOG(3) << "Extraction cancelled; no history_url_ value.";
    return;
  }

  size_t loading_tab_count =
      resource_coordinator::TabLoadTracker::Get()->GetLoadingTabCount();
  if (loading_tab_count > 0) {
    // Not ready yet. Try again after the delay.
    if (!ScheduleExtraction(weak_render_frame_host)) {
      RecordExtractionCancelled(
          ExtractionCancelled::TAB_HELPER_EXTRACT_PASSAGES_RESCHEDULE);
      VLOG(3) << "Extraction cancelled; " << loading_tab_count
              << " tabs still loading.";
    } else {
      VLOG(3) << "Extraction rescheduled; " << loading_tab_count
              << " tabs still loading.";
    }
    return;
  }

  if (history::HistoryService* history_service = GetHistoryService()) {
    // Callback is a member method instead of inline to enable cancellation via
    // weak pointer in `CancelExtraction()`.
    history_service->GetMostRecentVisitsForGurl(
        history_url_.value(), 1, history::VisitQuery404sPolicy::kExclude404s,
        base::BindOnce(
            &HistoryEmbeddingsTabHelper::ExtractPassagesWithHistoryData,
            weak_ptr_factory_.GetWeakPtr(), weak_render_frame_host),
        &task_tracker_);
  }
}

void HistoryEmbeddingsTabHelper::ExtractPassagesWithHistoryData(
    content::WeakDocumentPtr weak_render_frame_host,
    history::QueryURLAndVisitsResult result) {
  // `visits` can be empty for navigations that don't result in a
  // visit being added to the DB, e.g. navigations to
  // "chrome://" URLs.
  if (!result.success || result.visits.empty()) {
    RecordExtractionCancelled(
        ExtractionCancelled::
            TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_RESULTS);
    return;
  }
  const history::URLRow& url_row = result.row;
  const history::VisitRow& latest_visit = result.visits[0];
  CHECK(url_row.id());
  CHECK(latest_visit.visit_id);
  CHECK_EQ(url_row.id(), latest_visit.url_id);
  // Make sure the visit we got actually corresponds to the
  // navigation by comparing the visit_times.
  if (!history_visit_time_.has_value() ||
      latest_visit.visit_time != *history_visit_time_) {
    RecordExtractionCancelled(
        ExtractionCancelled::
            TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_TIME);
    return;
  }
  // Make sure the latest visit (the first one in the array) is
  // a local one. That should almost always be the case, since
  // this gets called just after a local visit happened, but in
  // some rare cases it might not be, e.g. if another device
  // sent us a visit "from the future". If this turns out to be
  // a problem, consider implementing a
  // GetMostRecent*Local*VisitsForURL().
  if (!latest_visit.originator_cache_guid.empty()) {
    RecordExtractionCancelled(
        ExtractionCancelled::
            TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_GUID);
    return;
  }

  RetrievePassages(latest_visit.url_id, latest_visit.visit_id,
                   latest_visit.visit_time, weak_render_frame_host);

  // Clear the data. It isn't reused and will be set anew by later navigation.
  history_visit_time_.reset();
  history_url_.reset();
}

void HistoryEmbeddingsTabHelper::RetrievePassages(
    history::URLID url_id,
    history::VisitID visit_id,
    base::Time visit_time,
    content::WeakDocumentPtr weak_render_frame_host) {
  content::RenderFrameHost* render_frame_host =
      weak_render_frame_host.AsRenderFrameHostIfValid();
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive()) {
    RecordExtractionCancelled(ExtractionCancelled::SERVICE_RETRIEVE_PASSAGES);
    return;
  }

  mojo::Remote<blink::mojom::InnerTextAgent> agent;
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      agent.BindNewPipeAndPassReceiver());
  auto params = blink::mojom::InnerTextParams::New();
  params->max_words_per_aggregate_passage =
      std::max(0, history_embeddings::GetFeatureParameters()
                      .passage_extraction_max_words_per_aggregate_passage);
  params->max_passages =
      history_embeddings::GetFeatureParameters().max_passages_per_page;
  params->min_words_per_passage = history_embeddings::GetFeatureParameters()
                                      .search_passage_minimum_word_count;
  auto* agent_ptr = agent.get();
  agent_ptr->GetInnerText(
      std::move(params),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &OnGotInnerText, std::move(agent),
              base::UTF16ToUTF8(GetWebContents().GetTitle()),
              base::ElapsedTimer(),
              base::BindOnce(&history_embeddings::HistoryEmbeddingsService::
                                 ComputeAndStorePassageEmbeddings,
                             GetHistoryEmbeddingsService()->AsWeakPtr(), url_id,
                             visit_id, visit_time)),
          nullptr));
}

void HistoryEmbeddingsTabHelper::CancelExtraction() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  task_tracker_.TryCancelAll();
}

history_embeddings::HistoryEmbeddingsService*
HistoryEmbeddingsTabHelper::GetHistoryEmbeddingsService() {
  CHECK(web_contents());
  return HistoryEmbeddingsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

passage_embeddings::PassageEmbedderModelObserver*
HistoryEmbeddingsTabHelper::GetPassageEmbedderModelObserver() {
  CHECK(web_contents());
  return passage_embeddings::PassageEmbedderModelObserverFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

history::HistoryService* HistoryEmbeddingsTabHelper::GetHistoryService() {
  CHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return HistoryServiceFactory::GetForProfileIfExists(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryEmbeddingsTabHelper);
