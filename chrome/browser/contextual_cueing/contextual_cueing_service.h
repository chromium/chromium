// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_

#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace tabs {
enum class GlicNudgeActivity;
}  // namespace tabs

namespace contextual_cueing {

class ContextualCueingService : public KeyedService {
 public:
  ContextualCueingService();
  ~ContextualCueingService() override;

  // Reports a page load happened to `url`, and is used to keep track of quiet
  // page loads requirement after a cueing UI is shown.
  void ReportPageLoad(const GURL& url);

  // Called when cueing nudge activity happens.
  void OnNudgeActivity(const GURL& url,
                       ukm::SourceId source_id,
                       tabs::GlicNudgeActivity activity);

  // Should be called when the cueing UI is shown.
  void CueingNudgeShown();

  // Should be called when the cueing UI is dismissed by the user.
  void CueingNudgeDismissed();

  // Should be called when the nudge is clicked on by the user.
  void CueingNudgeClicked();

  // Returns if a nudge should be shown and is not blocked by feature
  // engagement constraints, and if not, why.
  NudgeDecision CanShowNudge();

  base::WeakPtr<ContextualCueingService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Returns true if nudge should not be shown due to the backoff rule.
  bool IsNudgeBlockedByBackoffRule() const;

  // Returns true if nudge should not be shown due to hard nudge cap
  // (i.e. x nudges per y hours).
  bool IsNudgeBlockedByNudgeCap() const;

  // Keeps track of timestamps of recent nudges for the sake of capping nudge
  // count over a period of time. This queue is maintained such that it only has
  // timestamps necessary to enforce the limits. Old timestamps will be trimmed.
  base::queue<base::Time> recent_nudge_timestamps_;

  // Number of times the cueing nudge has been dismissed (i.e. closed by the
  // user). This count resets to 0 if nudge is clicked on by the user.
  int dismiss_count_ = 0;

  // The last time the cueing nudge was dismissed.
  std::optional<base::Time> backoff_end_time_;

  // A counter for how many subsequent page load events will be prevented from
  // showing a nudge. This is to limit the frequency at which consecutive page
  // loads can trigger nudges.
  size_t remaining_quiet_loads_ = 0;

  base::WeakPtrFactory<ContextualCueingService> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
