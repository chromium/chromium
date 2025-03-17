// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_

#include <vector>

#include "base/containers/lru_cache.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/nudge_cap_tracker.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;

namespace tabs {
enum class GlicNudgeActivity;
}  // namespace tabs

namespace contextual_cueing {

class ContextualCueingService
    : public KeyedService,
      page_content_annotations::PageContentExtractionService::Observer {
 public:
  explicit ContextualCueingService(
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service);
  ~ContextualCueingService() override;

  // Reports a page load happened to `url`, and is used to keep track of quiet
  // page loads requirement after a cueing UI is shown.
  void ReportPageLoad();

  // Called when cueing nudge activity happens.
  void OnNudgeActivity(const GURL& url,
                       ukm::SourceId source_id,
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

 private:
  // page_content_annotations::PageContentExtractionService::Observer:
  void OnPageContentExtracted(
      content::Page& page,
      const optimization_guide::proto::AnnotatedPageContent& page_content)
      override;

  // Returns true if nudge should not be shown due to the backoff rule.
  bool IsNudgeBlockedByBackoffRule() const;

  // Tracker to limit the number of nudges shown over a certain duration.
  NudgeCapTracker recent_nudge_tracker_;

  // Number of times the cueing nudge has been dismissed (i.e. closed by the
  // user). This count resets to 0 if nudge is clicked on by the user.
  int dismiss_count_ = 0;

  // The last time the cueing nudge was dismissed.
  std::optional<base::Time> backoff_end_time_;

  // A counter for how many subsequent page load events will be prevented from
  // showing a nudge. This is to limit the frequency at which consecutive page
  // loads can trigger nudges.
  size_t remaining_quiet_loads_ = 0;

  // Maintains the recently visited origins along with their nudge cap tracking.
  base::LRUCache<url::Origin, NudgeCapTracker> recent_visited_origins_;

  raw_ptr<page_content_annotations::PageContentExtractionService>
      page_content_extraction_service_ = nullptr;

  base::WeakPtrFactory<ContextualCueingService> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
