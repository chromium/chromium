// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/metrics/push_notification_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace {

const char kTotalTokenRetrievalTime[] =
    "PushNotification.ChromeOS.GCM.Token.RetrievalTime";

}  // namespace

namespace push_notification::metrics {

void RecordPushNotificationServiceTimeToRetrieveToken(
    base::TimeDelta total_retrieval_time) {
  base::UmaHistogramTimes(kTotalTokenRetrievalTime, total_retrieval_time);
}

}  // namespace push_notification::metrics
