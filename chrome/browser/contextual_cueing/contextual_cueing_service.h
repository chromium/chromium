// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_

#include <vector>

#include "base/containers/lru_cache.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/nudge_cap_tracker.h"
#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;
class OptimizationGuideKeyedService;
class PrefService;
class TemplateURLService;

namespace content {
class WebContents;
}  // namespace content

namespace predictors {
class LoadingPredictor;
}  // namespace predictors

namespace tabs {
enum class GlicNudgeActivity;
}  // namespace tabs

namespace contextual_cueing {

class ContextualCueingService
    : public KeyedService,
      page_content_annotations::PageContentExtractionService::Observer {
 public:
  ContextualCueingService(
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service,
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      predictors::LoadingPredictor* loading_predictor,
      PrefService* pref_service,
      TemplateURLService* template_url_service);
  ~ContextualCueingService() override;

  // Reports a page load happened to `url`, and is used to keep track of quiet
  // page loads requirement after a cueing UI is shown.
  void ReportPageLoad();

  // Called when cueing nudge activity happens.
  void OnNudgeActivity(content::WebContents* web_contents,
                       base::TimeTicks document_available_time,
                       tabs::GlicNudgeActivity activity);

  // Should be called when the cueing UI is shown for the tab with `url`.
  void CueingNudgeShown(const GURL& url);

  // Should be called when the cueing UI is dismissed by the user.
  void CueingNudgeDismissed();

  // Should be called when the nudge is clicked on by the user.
  void CueingNudgeClicked();

  // Returns if a nudge should be shown and is not blocked by feature
  // engagement constraints for navigation to `url`, and if not, why.
  NudgeDecision CanShowNudge(const GURL& url);

  base::WeakPtr<ContextualCueingService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Informs `this` to prepare fetching for zero state suggestions for GLIC.
  // Note that this *will not* actually do the fetch and it is intended for the
  // caller to call `GetContextualGlicZeroStateSuggestions` to actually fetch
  // the suggestions.
  void PrepareToFetchContextualGlicZeroStateSuggestions(
      content::WebContents* web_contents);

  // Returns zero state suggestions for GLIC. Virtual for testing.
  virtual void GetContextualGlicZeroStateSuggestions(
      content::WebContents* web_contents,
      bool is_fre,
      GlicSuggestionsCallback callback);

 private:
  // page_content_annotations::PageContentExtractionService::Observer:
  void OnPageContentExtracted(
      content::Page& page,
      const optimization_guide::proto::AnnotatedPageContent& page_content)
      override;

  // Called when suggestions are received. Cleans up after suggestions
  // generation.
  void OnSuggestionsReceived(
      base::TimeTicks fetch_begin_time,
      GlicSuggestionsCallback callback,
      std::optional<std::vector<std::string>> suggestions);

  // Returns true if nudge should not be shown due to the backoff rule.
  bool IsNudgeBlockedByBackoffRule() const;

  // Returns true if the given url is of a page type eligible for contextual
  // suggestions.
  bool IsPageTypeEligibleForContextualSuggestions(GURL url) const;

  // Tracker to limit the number of nudges shown over a certain duration.
  NudgeCapTracker recent_nudge_tracker_;

  // Number of times the cueing nudge has been dismissed (i.e. closed by the
  // user). This count resets to 0 if nudge is clicked on by the user.
  int dismiss_count_ = 0;

  // The end of the backoff period triggered by the last nudge dismissal.
  std::optional<base::TimeTicks> dismiss_backoff_end_time_;

  // The end of the backoff period triggered by the last shown nudge.
  std::optional<base::TimeTicks> shown_backoff_end_time_;

  // A counter for how many subsequent page load events will be prevented from
  // showing a nudge. This is to limit the frequency at which consecutive page
  // loads can trigger nudges.
  size_t remaining_quiet_loads_ = 0;

  // Maintains the recently visited origins along with their nudge cap tracking.
  base::LRUCache<url::Origin, NudgeCapTracker> recent_visited_origins_;

  raw_ptr<page_content_annotations::PageContentExtractionService>
      page_content_extraction_service_ = nullptr;

  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;

  raw_ptr<predictors::LoadingPredictor> loading_predictor_ = nullptr;

  raw_ptr<PrefService> pref_service_ = nullptr;

  raw_ptr<TemplateURLService> template_url_service_ = nullptr;

  // Stores model execution url to save look up time.
  GURL mes_url_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ContextualCueingService> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
