// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_METRICS_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_METRICS_H_

#include "base/time/time.h"

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class KAnonymityServiceJoinSetAction {
  kJoinSet = 0,
  kJoinSetSuccess = 1,
  kFetchJoinSetOHTTPKey = 2,
  kFetchJoinSetOHTTPKeyFailed = 3,
  kSendJoinSetRequest = 4,
  kJoinSetRequestFailed = 5,
  kJoinSetQueueFull = 6,
  kMaxValue = 6,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class KAnonymityServiceQuerySetAction {
  kQuerySet = 0,
  kQuerySetsSuccess = 1,
  kFetchQuerySetOHTTPKey = 2,
  kFetchQuerySetOHTTPKeyFailed = 3,
  kSendQuerySetRequest = 4,
  kQuerySetRequestFailed = 5,
  kQuerySetQueueFull = 6,
  kQuerySetRequestParseError = 7,
  kMaxValue = 7,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class KAnonymityTrustTokenGetterAction {
  kTryGetTrustTokenAndKey = 0,
  kGetTrustTokenSuccess = 1,
  kRequestAccessToken = 2,
  kAccessTokenRequestFailed = 3,
  kFetchNonUniqueClientID = 4,
  kFetchNonUniqueClientIDFailed = 5,
  kFetchNonUniqueClientIDParseError = 6,
  kFetchTrustTokenKey = 7,
  kFetchTrustTokenKeyFailed = 8,
  kFetchTrustTokenKeyParseError = 9,
  kFetchTrustToken = 10,
  kFetchTrustTokenFailed = 11,
  kMaxValue = 11,
};

void RecordJoinSetAction(KAnonymityServiceJoinSetAction action);

void RecordQuerySetAction(KAnonymityServiceQuerySetAction action);

void RecordQuerySetSize(size_t size);

void RecordTrustTokenGetterAction(KAnonymityTrustTokenGetterAction action);

void RecordJoinSetLatency(base::TimeTicks request_start,
                          base::TimeTicks request_end);

void RecordQuerySetLatency(base::TimeTicks request_start,
                           base::TimeTicks request_end);

void RecordTrustTokenGet(base::TimeTicks request_start,
                         base::TimeTicks request_end);

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_METRICS_H_
