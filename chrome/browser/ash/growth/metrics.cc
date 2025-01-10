// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/metrics.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/campaigns_utils.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

constexpr char kButtonPressedHistogramName[] =
    "Ash.Growth.Ui.ButtonPressed.Button%d.Campaigns%d";

constexpr char kDismissedHistogramName[] =
    "Ash.Growth.Ui.Dismissed.Campaigns%d";

constexpr char kImpressionHistogramName[] =
    "Ash.Growth.Ui.Impression.Campaigns%d";

std::string GetButtonPressedHistogramName(int campaign_id,
                                          CampaignButtonId button_id) {
  // E.g. "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500".
  return base::StringPrintf(kButtonPressedHistogramName,
                            static_cast<int>(button_id),
                            growth::GetHistogramMaxCampaignId(campaign_id));
}

std::string GetDismissedHistogramName(int campaign_id) {
  // E.g. "Ash.Growth.Ui.Dismissed.Campaigns500".
  return base::StringPrintf(kDismissedHistogramName,
                            growth::GetHistogramMaxCampaignId(campaign_id));
}

std::string GetImpressionHistogramName(int campaign_id) {
  // E.g. "Ash.Growth.Ui.Impression.Campaigns500".
  return base::StringPrintf(kImpressionHistogramName,
                            growth::GetHistogramMaxCampaignId(campaign_id));
}

}  // namespace

void RecordButtonPressed(int campaign_id,
                         CampaignButtonId button_id,
                         bool should_log_cros_events) {
  CAMPAIGNS_LOG(DEBUG) << "Campaign_id: " << campaign_id
                       << " button_id: " << static_cast<int>(button_id);
  const std::string histogram_name =
      GetButtonPressedHistogramName(campaign_id, button_id);
  base::UmaHistogramSparse(histogram_name, campaign_id);

  if (ash::features::IsGrowthCampaignsCrOSEventsEnabled() &&
      should_log_cros_events) {
    metrics::structured::StructuredMetricsClient::Record(
        std::move(cros_events::Growth_Ui_ButtonPressed()
                      .SetCampaignId(campaign_id)
                      .SetButtonId(static_cast<cros_events::CampaignButtonId>(
                          button_id))));
  }
}

void RecordDismissed(int campaign_id, bool should_log_cros_events) {
  const std::string histogram_name = GetDismissedHistogramName(campaign_id);
  CAMPAIGNS_LOG(DEBUG) << "Campaign_id: " << campaign_id;
  base::UmaHistogramSparse(histogram_name, campaign_id);

  if (ash::features::IsGrowthCampaignsCrOSEventsEnabled() &&
      should_log_cros_events) {
    metrics::structured::StructuredMetricsClient::Record(std::move(
        cros_events::Growth_Ui_Dismissed().SetCampaignId(campaign_id)));
  }
}

void RecordImpression(int campaign_id, bool should_log_cros_events) {
  const std::string histogram_name = GetImpressionHistogramName(campaign_id);
  CAMPAIGNS_LOG(DEBUG) << "Campaign_id: " << campaign_id;
  base::UmaHistogramSparse(histogram_name, campaign_id);

  if (ash::features::IsGrowthCampaignsCrOSEventsEnabled() &&
      should_log_cros_events) {
    metrics::structured::StructuredMetricsClient::Record(std::move(
        cros_events::Growth_Ui_Impression().SetCampaignId(campaign_id)));
  }
}
