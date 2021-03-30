// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {

// Returns a suffix to be used in UMA histogram names. Needs to be kept in sync
// with token entries of Notifications.macOS.{ActionReceived,Delivered} metrics
// in //tools/metrics/histograms/histograms_xml/notifications/histograms.xml.
std::string NotificationStyleSuffix(bool is_alert) {
  return is_alert ? "Alert" : "Banner";
}

}  // namespace

void LogMacNotificationActionReceived(bool is_alert, bool is_valid) {
  base::UmaHistogramBoolean(base::StrCat({"Notifications.macOS.ActionReceived.",
                                          NotificationStyleSuffix(is_alert)}),
                            is_valid);
}

void LogMacNotificationDelivered(bool is_alert, bool success) {
  base::UmaHistogramBoolean(base::StrCat({"Notifications.macOS.Delivered.",
                                          NotificationStyleSuffix(is_alert)}),
                            success);
}
