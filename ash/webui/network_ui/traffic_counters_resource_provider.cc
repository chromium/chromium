// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/network_ui/traffic_counters_resource_provider.h"

#include "ash/constants/ash_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {
namespace traffic_counters {

namespace {

constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"TrafficCountersUnknown", IDS_TRAFFIC_COUNTERS_UNKNOWN},
    {"TrafficCountersChrome", IDS_TRAFFIC_COUNTERS_CHROME},
    {"TrafficCountersUser", IDS_TRAFFIC_COUNTERS_USER},
    {"TrafficCountersArc", IDS_TRAFFIC_COUNTERS_ARC},
    {"TrafficCountersCrosvm", IDS_TRAFFIC_COUNTERS_CROSVM},
    {"TrafficCountersPluginvm", IDS_TRAFFIC_COUNTERS_PLUGINVM},
    {"TrafficCountersUpdateEngine", IDS_TRAFFIC_COUNTERS_UPDATE_ENGINE},
    {"TrafficCountersVpn", IDS_TRAFFIC_COUNTERS_VPN},
    {"TrafficCountersSystem", IDS_TRAFFIC_COUNTERS_SYSTEM},
    {"TrafficCountersGuid", IDS_TRAFFIC_COUNTERS_GUID},
    {"TrafficCountersName", IDS_TRAFFIC_COUNTERS_NAME},
    {"TrafficCountersTrafficCounters", IDS_TRAFFIC_COUNTERS_TRAFFIC_COUNTERS},
    {"TrafficCountersRequestTrafficCounters",
     IDS_TRAFFIC_COUNTERS_REQUEST_TRAFFIC_COUNTERS},
    {"TrafficCountersResetTrafficCounters",
     IDS_TRAFFIC_COUNTERS_RESET_TRAFFIC_COUNTERS},
    {"TrafficCountersLastResetTime", IDS_TRAFFIC_COUNTERS_LAST_RESET_TIME},
    // Settings UI
    {"TrafficCountersDataUsageLabel", IDS_TRAFFIC_COUNTERS_DATA_USAGE_LABEL},
    {"TrafficCountersDataUsageSinceLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_SINCE_LABEL},
    {"TrafficCountersDataUsageResetButtonLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_RESET_BUTTON_LABEL},
    {"TrafficCountersDataUsageResetLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_RESET_LABEL},
    {"TrafficCountersDataUsageEnableAutoResetLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_ENABLE_AUTO_RESET_LABEL},
    {"TrafficCountersDataUsageLastResetDateUnavailableLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_LAST_RESET_DATE_UNAVAILABLE_LABEL},
    {"TrafficCountersDataUsageEnableAutoResetSublabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_ENABLE_AUTO_RESET_SUBLABEL},
    {"TrafficCountersDataUsageAutoResetDayOfMonthLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_AUTO_RESET_DAY_OF_MONTH_LABEL},
    {"TrafficCountersDataUsageAutoResetDayOfMonthSubLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_AUTO_RESET_DAY_OF_MONTH_SUBLABEL},
    {"TrafficCountersDataUsageDifferentFromProviderLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_DIFFERENT_FROM_PROVIDER_LABEL},
    {"TrafficCountersDataUsageResetDayTooltipText",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_RESET_DAY_TOOLTIP_TEXT},
    {"TrafficCountersDataUsageDropdownLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_DROPDOWN_LABEL},
    {"TrafficCountersDataUsageResetDayTooltipA11yLabel",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_RESET_DAY_TOOLTIP_A11Y_LABEL},
    {"TrafficCountersDataUsageResetButtonPressedA11yMessage",
     IDS_TRAFFIC_COUNTERS_DATA_USAGE_RESET_BUTTON_PRESSED_A11Y_MESSAGE},
};

}  // namespace

void AddResources(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean("trafficCountersEnabled",
                          ash::features::IsTrafficCountersEnabled());
}

}  // namespace traffic_counters
}  // namespace ash
