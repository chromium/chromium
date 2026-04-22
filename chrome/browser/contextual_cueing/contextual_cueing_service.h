// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_

#include <optional>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/contextual_cueing/nudge_cap_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace contextual_cueing {

enum class ContextualCueingDecision;

class ContextualCueingService : public KeyedService {
 public:
  ContextualCueingService();
  ~ContextualCueingService() override;

  // Reports a page load occurred. This is used to keep track of quiet
  // page loads requirement after a cueing UI is shown.
  void ReportPageLoad();

  // Called when the user clicks the cue action button.
  void OnCueClicked(CueTargetType type);

  // Called when the user dismisses the cue.
  void OnCueDismissed(CueTargetType type);

  // Called when the cue is shown to the user.
  void OnCueShown(const GURL& url);

  // Returns true if a nudge can be shown.
  ContextualCueingDecision CanShowCue(const GURL& url) const;

 private:
  // A counter for how many subsequent page load events will be prevented from
  // showing a nudge. This is to limit the frequency at which consecutive page
  // loads can trigger nudges.
  size_t remaining_quiet_loads_ = 0;

  // The end of the backoff period triggered by the last shown nudge.
  std::optional<base::TimeTicks> shown_backoff_end_time_;

  // Number of times the cueing nudge has been dismissed (i.e. closed by the
  // user). This count resets to 0 if nudge is clicked on by the user.
  int dismiss_count_ = 0;

  // The end of the backoff period triggered by the last dismissed nudge.
  std::optional<base::TimeTicks> dismiss_backoff_end_time_;

  // Tracker to limit the number of nudges shown over a certain duration.
  NudgeCapTracker recent_nudge_tracker_;

  // Maintains the recently visited origins along with their nudge cap tracking.
  base::LRUCache<url::Origin, NudgeCapTracker> recent_visited_origins_;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
