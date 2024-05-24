// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/metrics/push_notification_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace {

const char kTotalTokenRetrievalTime[] =
    "PushNotification.ChromeOS.GCM.Token.RetrievalTime";
const char kTotalSuccessfulRegistrationResponseTime[] =
    "PushNotification.ChromeOS.MultiLoginUpdateApi.ResponseTime.Success";
const char kTotalFailedRegistrationResponseTime[] =
    "PushNotification.ChromeOS.MultiLoginUpdateApi.ResponseTime.Failure";
const char kGcmTokenRetrievalResult[] =
    "PushNotification.ChromeOS.GCM.Token.RetrievalResult";
const char kOAuthTokenRetrievalResult[] =
    "PushNotification.ChromeOS.OAuth.Token.RetrievalResult";
const char kServiceRegistrationResult[] =
    "PushNotification.ChromeOS.Registration.Result";

}  // namespace

namespace push_notification::metrics {

void RecordPushNotificationServiceTimeToRetrieveToken(
    base::TimeDelta total_retrieval_time) {
  base::UmaHistogramTimes(kTotalTokenRetrievalTime, total_retrieval_time);
}

void RecordPushNotificationServiceTimeToReceiveRegistrationSuccessResponse(
    base::TimeDelta registration_response_time) {
  base::UmaHistogramTimes(kTotalSuccessfulRegistrationResponseTime,
                          registration_response_time);
}

void RecordPushNotificationServiceTimeToReceiveRegistrationFailureResponse(
    base::TimeDelta registration_response_time) {
  base::UmaHistogramTimes(kTotalFailedRegistrationResponseTime,
                          registration_response_time);
}

void RecordPushNotificationGcmTokenRetrievalResult(bool success) {
  base::UmaHistogramBoolean(kGcmTokenRetrievalResult, success);
}

void RecordPushNotificationOAuthTokenRetrievalResult(bool success) {
  base::UmaHistogramBoolean(kOAuthTokenRetrievalResult, success);
}

void RecordPushNotificationServiceRegistrationResult(bool success) {
  base::UmaHistogramBoolean(kServiceRegistrationResult, success);
}

}  // namespace push_notification::metrics
