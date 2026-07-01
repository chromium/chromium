// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/private_insights/private_insights_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/metrics/private_metrics/private_insights/contextual_cue_log_event_helpers.h"
#include "components/metrics/private_metrics/private_insights/events/contextual_cue_log_event.pb.h"
#include "components/metrics/private_metrics/private_insights/private_insights_features.h"
#include "components/metrics/private_metrics/private_insights/private_insights_service.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace contextual_cueing {

namespace {

std::string InteractionTypeToString(ContextualCueingInteraction interaction) {
  switch (interaction) {
    case ContextualCueingInteraction::kCueClicked:
      return "Clicked";
    case ContextualCueingInteraction::kCueDismissed:
      return "Dismissed";
    case ContextualCueingInteraction::kCueEditPrompt:
      return "EditPrompt";
    case ContextualCueingInteraction::kCueSuggestionsSettings:
      return "Settings";
  }
}

int BucketTabCount(int raw_count) {
  return ukm::GetExponentialBucketMin(raw_count, 1.5);
}

void PopulateSystemProfile(
    private_insights::events::ContextualCueLogEvent::SystemProfile* profile) {
  profile->set_chrome_version(version_info::GetVersionNumber());
  profile->set_milestone(version_info::GetMajorVersionNumber());
  profile->set_chrome_channel(
      std::string(version_info::GetChannelString(chrome::GetChannel())));
  if (g_browser_process) {
    profile->set_language(g_browser_process->GetApplicationLocale());
    if (auto* variations_service = g_browser_process->variations_service()) {
      profile->set_country(variations_service->GetStoredPermanentCountry());
    }
  }
}

}  // namespace

void RecordCueShownMetrics(ukm::SourceId source_id,
                           std::string_view cuj,
                           const CueTabMetrics& tab_metrics,
                           base::TimeDelta latency) {
  base::UmaHistogramSparse("ContextualCueing.V2.CueShown",
                           base::HashMetricName(cuj));
  base::UmaHistogramTimes("ContextualCueing.V2.CueShownLatency", latency);

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::ContextualCueing_CueShown(source_id)
      .SetSuggestedCujCategory(base::HashMetricName(cuj))
      .SetMatchedTabCount(BucketTabCount(tab_metrics.matched_count))
      .SetMissingTabCount(BucketTabCount(tab_metrics.missing_count))
      .SetNavigatedAwayTabCount(
          BucketTabCount(tab_metrics.navigated_away_count))
      .SetProactiveCueLatencyAfterPageLoad(latency.InMilliseconds())
      .SetProactiveCueDecision(
          static_cast<int64_t>(ContextualCueingDecision::kSuccess))
      .Record(ukm_recorder);
}

void RecordContextualCueingInteraction(
    ContextualCueingInteraction contextual_cueing_interaction,
    const std::string& cuj,
    ukm::SourceId source_id,
    base::TimeDelta shown_duration) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.CueInteraction",
                                contextual_cueing_interaction);

  std::string histogram_name =
      "ContextualCueing.V2.CueInteraction." +
      InteractionTypeToString(contextual_cueing_interaction);
  base::UmaHistogramSparse(histogram_name, base::HashMetricName(cuj));

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::ContextualCueing_CueInteraction(source_id)
      .SetProactiveCueShownDurationMs(ukm::GetExponentialBucketMinForUserTiming(
          shown_duration.InMilliseconds()))
      .SetProactiveCueInteraction(
          static_cast<int64_t>(contextual_cueing_interaction))
      .Record(ukm_recorder);
}

void RecordContextualCueingDecision(
    ukm::SourceId source_id,
    ContextualCueingDecision contextual_cueing_decision) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.Decision",
                                contextual_cueing_decision);

  // If the decision is kSuccess, RecordCueShownMetrics will record the
  // ProactiveCueDecision UKM instead.
  if (contextual_cueing_decision != ContextualCueingDecision::kSuccess) {
    auto* ukm_recorder = ukm::UkmRecorder::Get();
    ukm::builders::ContextualCueing_CueShown(source_id)
        .SetProactiveCueDecision(
            static_cast<int64_t>(contextual_cueing_decision))
        .Record(ukm_recorder);
  }
}

void RecordCueFormFactorShown(CueFormFactor form_factor) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.CueFormFactor.Shown",
                                form_factor);
}

void RecordCueFormFactorHidden(CueFormFactor form_factor) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.CueFormFactor.Hidden",
                                form_factor);
}

void RecordChipClickedCollapsedDuration(base::TimeDelta collapsed_duration) {
  base::UmaHistogramLongTimes(
      "ContextualCueing.V2.ChipClicked.CollapsedDuration", collapsed_duration);
}

void RecordCueShownToPrivateInsights(
    Profile* profile,
    const std::string& cue_id,
    CueTargetType cue_type,
    const optimization_guide::proto::ContextualCue& cue,
    tabs::TabInterface* active_tab,
    const std::vector<tabs::TabHandle>& tabs_to_show) {
  if (!kEnablePrivateInsightsLogging.Get()) {
    return;
  }
  auto* private_insights_service =
      private_insights::PrivateInsightsServiceFactory::GetForProfile(profile);
  if (!private_insights_service) {
    return;
  }

  private_insights::events::ContextualCueLogEvent event;
  event.set_cue_id(cue_id);
  event.set_event_type(private_insights::events::ContextualCueLogEvent::SHOWN);
  event.set_event_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());

  PopulateSystemProfile(event.mutable_system_profile());

  if (active_tab->GetContents()) {
    event.mutable_cue_context()->mutable_active_page()->set_url(
        active_tab->GetContents()->GetLastCommittedURL().spec());
    event.mutable_cue_context()->mutable_active_page()->set_title(
        base::UTF16ToUTF8(active_tab->GetContents()->GetTitle()));
  }

  std::vector<private_insights::events::ContextualCueLogEvent::PageInfo>
      recent_pages;
  for (const auto& handle : tabs_to_show) {
    if (auto* tab = handle.Get()) {
      if (tab->GetContents()) {
        private_insights::events::ContextualCueLogEvent::PageInfo page_info;
        page_info.set_url(tab->GetContents()->GetLastCommittedURL().spec());
        page_info.set_title(base::UTF16ToUTF8(tab->GetTitle()));
        recent_pages.push_back(std::move(page_info));
      }
    }
  }
  event.mutable_cue_context()->set_recent_pages(
      private_insights::SerializePageInfoListToJson(recent_pages));

  event.mutable_cue_details()->set_cuj_type(cue.suggested_cuj());
  event.mutable_cue_details()->set_suggestion_text(
      cue.anchored_message_cue().anchored_message_text());
  event.mutable_cue_details()->set_promoted_feature(GetName(cue_type));
  event.mutable_cue_details()->set_button_text(
      cue.anchored_message_cue().action_text());
  if (cue.has_gemini_in_chrome_surface()) {
    event.mutable_cue_details()->set_prompt(
        cue.gemini_in_chrome_surface().prompt());
  }

  private_insights_service->LogContextualCueEvent(std::move(event));
}

void RecordCueingInteractionToPrivateInsights(
    Profile* profile,
    const std::string& cue_id,
    ContextualCueingInteraction interaction_type,
    const std::string& cuj) {
  if (!kEnablePrivateInsightsLogging.Get()) {
    return;
  }
  auto* private_insights_service =
      private_insights::PrivateInsightsServiceFactory::GetForProfile(profile);
  if (!private_insights_service) {
    return;
  }

  private_insights::events::ContextualCueLogEvent event;
  event.set_cue_id(cue_id);
  event.set_event_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  PopulateSystemProfile(event.mutable_system_profile());

  // Map interaction_type to event_type
  switch (interaction_type) {
    case ContextualCueingInteraction::kCueClicked:
      event.set_event_type(
          private_insights::events::ContextualCueLogEvent::CLICKED);
      break;
    case ContextualCueingInteraction::kCueDismissed:
      event.set_event_type(
          private_insights::events::ContextualCueLogEvent::DISMISSED);
      break;
    case ContextualCueingInteraction::kCueSuggestionsSettings:
      event.set_event_type(
          private_insights::events::ContextualCueLogEvent::CLICKED_SETTINGS);
      break;
    case ContextualCueingInteraction::kCueEditPrompt:
      event.set_event_type(
          private_insights::events::ContextualCueLogEvent::EDIT_PROMPT);
      break;
  }

  event.mutable_cue_details()->set_cuj_type(cuj);

  private_insights_service->LogContextualCueEvent(std::move(event));
}

}  // namespace contextual_cueing
