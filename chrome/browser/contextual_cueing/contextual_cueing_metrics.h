// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_METRICS_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_METRICS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/metrics/private_metrics/private_insights/events/contextual_cue_log_event.pb.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class Profile;

namespace contextual_cueing {

enum class ContextualCueingInteraction;
enum class ContextualCueingDecision;
enum class CueFormFactor;
enum class CueTargetType;

// Counts of tabs received from the contextual cue server and whether they are
// still relevant, or why they aren't.
struct CueTabMetrics {
  int matched_count = 0;
  int missing_count = 0;
  int navigated_away_count = 0;
};

void RecordCueShownMetrics(ukm::SourceId source_id,
                           std::string_view cuj,
                           const CueTabMetrics& tab_metrics,
                           base::TimeDelta latency);

void RecordContextualCueingInteraction(
    ContextualCueingInteraction contextual_cueing_interaction,
    const std::string& cuj,
    ukm::SourceId source_id,
    base::TimeDelta shown_duration);

void RecordContextualCueingDecision(
    ukm::SourceId source_id,
    ContextualCueingDecision contextual_cueing_decision);

void RecordCueFormFactorShown(CueFormFactor form_factor);
void RecordCueFormFactorHidden(CueFormFactor form_factor);
void RecordChipClickedCollapsedDuration(base::TimeDelta collapsed_duration);

void RecordCueShownToPrivateInsights(
    Profile* profile,
    const std::string& cue_id,
    CueTargetType cue_type,
    const optimization_guide::proto::ContextualCue& cue,
    tabs::TabInterface* active_tab,
    const std::vector<tabs::TabHandle>& tabs_to_show,
    const std::vector<optimization_guide::proto::Tab>& background_tabs);

void RecordCueingInteractionToPrivateInsights(
    Profile* profile,
    const std::string& cue_id,
    CueTargetType cue_type,
    const optimization_guide::proto::ContextualCue& cue,
    tabs::TabInterface* active_tab,
    const std::vector<tabs::TabHandle>& tabs_to_show,
    const std::vector<optimization_guide::proto::Tab>& background_tabs,
    ContextualCueingInteraction interaction_type,
    const std::string& cuj);

namespace internal {

// Builds the event structure without logging it.
// Exposed here so it can be verified in lightweight unit tests.
private_insights::events::ContextualCueLogEvent CreateContextualCueLogEvent(
    private_insights::events::ContextualCueLogEvent::EventType event_type,
    const std::string& cue_id,
    CueTargetType cue_type,
    const optimization_guide::proto::ContextualCue& cue,
    tabs::TabInterface* active_tab,
    const std::vector<tabs::TabHandle>& tabs_to_show,
    const std::vector<optimization_guide::proto::Tab>& background_tabs);

}  // namespace internal

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_METRICS_H_
