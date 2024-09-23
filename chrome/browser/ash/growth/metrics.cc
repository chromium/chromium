// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/metrics.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
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

constexpr int kCampaignsCountPerHistogram = 500;

// Get the max `campaign_id` (exclusive) can be recorded in the histogram.
// Campaign will be logged in a histogram named by rounding the `campaign_id`
// to the next five hundred. For examples:
//   `campaign_id`: 0 => "Ash.Growth.Ui.Dismissed.Campaigns500"
//   `campaign_id`: 100 => "Ash.Growth.Ui.Dismissed.Campaigns500"
//   `campaign_id`: 499 => "Ash.Growth.Ui.Dismissed.Campaigns500"
//   `campaign_id`: 500 => "Ash.Growth.Ui.Dismissed.Campaigns1000"
//   `campaign_id`: 501 => "Ash.Growth.Ui.Dismissed.Campaigns1000"
//   `campaign_id`: 1000 => "Ash.Growth.Ui.Dismissed.Campaigns1500"
//   `campaign_id`: 9999 => "Ash.Growth.Ui.Dismissed.Campaigns10000"
//   `campaign_id`: 10000 => "Ash.Growth.Ui.Dismissed.Campaigns10500"
int GetHistogramMaxCampaignId(int campaign_id) {
  // `campaign_id` starts at 0.
  return (campaign_id / kCampaignsCountPerHistogram + 1) *
         kCampaignsCountPerHistogram;
}

std::string GetButtonPressedHistogramName(int campaign_id,
                                          CampaignButtonId button_id) {
  // E.g. "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500".
  return base::StringPrintf(kButtonPressedHistogramName,
                            static_cast<int>(button_id),
                            GetHistogramMaxCampaignId(campaign_id));
}

std::string GetDismissedHistogramName(int campaign_id) {
  // E.g. "Ash.Growth.Ui.Dismissed.Campaigns500".
  return base::StringPrintf(kDismissedHistogramName,
                            GetHistogramMaxCampaignId(campaign_id));
}

std::string GetImpressionHistogramName(int campaign_id) {
  // E.g. "Ash.Growth.Ui.Impression.Campaigns500".
  return base::StringPrintf(kImpressionHistogramName,
                            GetHistogramMaxCampaignId(campaign_id));
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
