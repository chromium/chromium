// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_METRICS_PUSH_NOTIFICATION_METRICS_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_METRICS_PUSH_NOTIFICATION_METRICS_H_

#include "base/time/time.h"

namespace push_notification::metrics {

void RecordPushNotificationServiceTimeToRetrieveToken(
    base::TimeDelta total_retrieval_time);
void RecordPushNotificationServiceTimeToReceiveRegistrationSuccessResponse(
    base::TimeDelta registration_response_time);
void RecordPushNotificationServiceTimeToReceiveRegistrationFailureResponse(
    base::TimeDelta registration_response_time);
void RecordPushNotificationGcmTokenRetrievalResult(bool success);
void RecordPushNotificationOAuthTokenRetrievalResult(bool success);
void RecordPushNotificationServiceRegistrationResult(bool success);

}  // namespace push_notification::metrics

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_METRICS_PUSH_NOTIFICATION_METRICS_H_
