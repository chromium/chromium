// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace glic {

namespace {
int BytesToMB(uint64_t bytes) {
  return static_cast<int>(bytes / 1024 / 1024);
}

const base::FeatureParam<base::TimeDelta> kMemoryMetricsPeriod{
    &features::kGlicRecordMemoryFootprintMetrics, "period", base::Minutes(30)};
}  // namespace

GlicInstanceCoordinatorMetrics::GlicInstanceCoordinatorMetrics(
    DataProvider* data_provider,
    PrefService* pref_service)
    : data_provider_(data_provider) {
  CHECK(pref_service);
  is_pinned_ = pref_service->GetBoolean(prefs::kGlicPinnedToTabstrip);
  pref_registrar_.Init(pref_service);
  pref_registrar_.Add(
      prefs::kGlicPinnedToTabstrip,
      base::BindRepeating(&GlicInstanceCoordinatorMetrics::OnPinningPrefChanged,
                          base::Unretained(this)));
}

GlicInstanceCoordinatorMetrics::~GlicInstanceCoordinatorMetrics() {
  EndConcurrentVisibility();
}

void GlicInstanceCoordinatorMetrics::StartPeriodicMemoryMetricsRecording() {
  if (base::FeatureList::IsEnabled(
          features::kGlicRecordMemoryFootprintMetrics)) {
    memory_metrics_timer_.Start(
        FROM_HERE, kMemoryMetricsPeriod.Get(), this,
        &GlicInstanceCoordinatorMetrics::RecordPeriodicMemoryMetrics);
  }
}

void GlicInstanceCoordinatorMetrics::OnInstanceVisibilityChanged() {
  int visible_count = GetVisibleInstanceCount();

  if (visible_count >= 2) {
    // We are in a concurrent visibility period.
    if (!concurrent_visibility_start_time_.has_value()) {
      // Start new concurrent visibility period.
      concurrent_visibility_start_time_ = base::TimeTicks::Now();
      concurrent_visibility_peak_count_ = visible_count;
    } else {
      // Update peak if needed.
      concurrent_visibility_peak_count_ =
          std::max(concurrent_visibility_peak_count_, visible_count);
    }
  } else {
    EndConcurrentVisibility();
  }
}

void GlicInstanceCoordinatorMetrics::RecordSwitchConversationTarget(
    const std::optional<std::string>& requested_conversation_id,
    const std::optional<std::string>& target_instance_conversation_id,
    raw_ptr<GlicInstance> active_instance) {
  GlicSwitchConversationTarget target = GlicSwitchConversationTarget::kUnknown;
  if (!requested_conversation_id.has_value()) {
    target = GlicSwitchConversationTarget::kStartNewConversation;
  } else if (requested_conversation_id ==
             GetMostRecentlyActiveConversationId(active_instance)) {
    target = GlicSwitchConversationTarget::kSwitchedToLastActive;
  } else if (target_instance_conversation_id == requested_conversation_id) {
    target = GlicSwitchConversationTarget::kSwitchedToExistingInstance;
  } else {
    target = GlicSwitchConversationTarget::kSwitchedToNewInstance;
  }
  base::UmaHistogramEnumeration("Glic.Interaction.SwitchConversationTarget",
                                target);
}

void GlicInstanceCoordinatorMetrics::RecordActivateTabCandidateTabCount(
    size_t count) {
  base::UmaHistogramCounts100(
      "Glic.ActivateTabWithConversation.CandidateTabCount", count);
}

void GlicInstanceCoordinatorMetrics::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  std::string_view suffix = (level == base::MEMORY_PRESSURE_LEVEL_MODERATE)
                                ? ".ModeratePressure"
                                : ".CriticalPressure";
  RecordMemoryFootprint(suffix);
}

void GlicInstanceCoordinatorMetrics::RecordPeriodicMemoryMetrics() {
  RecordMemoryFootprint(".Periodic");
}

void GlicInstanceCoordinatorMetrics::RecordMemoryFootprint(
    std::string_view suffix) {
  if (!base::FeatureList::IsEnabled(
          features::kGlicRecordMemoryFootprintMetrics)) {
    return;
  }

  auto web_contents_list = data_provider_->GetAllUnhibernatedWebContents();
  if (web_contents_list.empty()) {
    return;
  }
  size_t instance_count = web_contents_list.size();

  uint64_t total_webui_bytes = 0;
  uint64_t total_client_bytes = 0;
  uint64_t absolute_total_bytes = 0;

  base::flat_set<content::RenderProcessHost*> unique_webui_processes;
  base::flat_set<content::RenderProcessHost*> unique_client_processes;

  for (const auto& item : web_contents_list) {
    if (item.webui_contents) {
      if (auto* process =
              item.webui_contents->GetPrimaryMainFrame()->GetProcess()) {
        unique_webui_processes.insert(process);
      }
    }
    if (item.web_client_contents) {
      if (auto* process =
              item.web_client_contents->GetPrimaryMainFrame()->GetProcess()) {
        unique_client_processes.insert(process);
      }
    }
  }

  for (auto* process : unique_webui_processes) {
    total_webui_bytes += process->GetPrivateMemoryFootprint();
  }
  for (auto* process : unique_client_processes) {
    total_client_bytes += process->GetPrivateMemoryFootprint();
  }
  // Assuming WebUI and Client processes are disjoint due to Site Isolation,
  // we can sum them to get the total.
  absolute_total_bytes = total_webui_bytes + total_client_bytes;

  int avg_webui_mb = BytesToMB(total_webui_bytes / instance_count);
  int avg_client_mb = BytesToMB(total_client_bytes / instance_count);
  int avg_total_mb = BytesToMB(absolute_total_bytes / instance_count);
  int total_mb = BytesToMB(absolute_total_bytes);

  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Glic.Instance.AvgWebUIPrivateMemoryFootprint", suffix}),
      avg_webui_mb);
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Glic.Instance.AvgClientPrivateMemoryFootprint", suffix}),
      avg_client_mb);
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Glic.Instance.AvgTotalPrivateMemoryFootprint", suffix}),
      avg_total_mb);
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Glic.Instance.TotalPrivateMemoryFootprint", suffix}),
      total_mb);
}

int GlicInstanceCoordinatorMetrics::GetVisibleInstanceCount() const {
  return data_provider_->GetVisibleInstanceCount();
}

void GlicInstanceCoordinatorMetrics::EndConcurrentVisibility() {
  if (concurrent_visibility_start_time_.has_value()) {
    // We were in a concurrent visibility period, but now we are not. End it.
    base::TimeDelta duration =
        base::TimeTicks::Now() - concurrent_visibility_start_time_.value();
    base::UmaHistogramLongTimes("Glic.ConcurrentVisibility.Duration", duration);
    base::UmaHistogramExactLinear("Glic.ConcurrentVisibility.PeakCount",
                                  concurrent_visibility_peak_count_, 10);

    // Reset state.
    concurrent_visibility_start_time_.reset();
    concurrent_visibility_peak_count_ = 0;
  }
}

std::optional<std::string>
GlicInstanceCoordinatorMetrics::GetMostRecentlyActiveConversationId(
    raw_ptr<GlicInstance> excluded_instance) {
  auto conversations = data_provider_->GetRecentlyActiveConversations(2);
  for (const auto& info : conversations) {
    if (info->conversation_id.empty()) {
      continue;
    }
    if (excluded_instance &&
        info->conversation_id ==
            excluded_instance->conversation_id().value_or("")) {
      continue;
    }
    return info->conversation_id;
  }
  return std::nullopt;
}

void GlicInstanceCoordinatorMetrics::OnPinningPrefChanged() {
  bool is_pinned =
      pref_registrar_.prefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  if (is_pinned == is_pinned_) {
    // No change, early exit.
    return;
  }
  is_pinned_ = is_pinned;
  if (is_pinned_) {
    base::RecordAction(base::UserMetricsAction("Glic.Pinned"));
  } else {
    base::RecordAction(base::UserMetricsAction("Glic.Unpinned"));
  }
}

}  // namespace glic
