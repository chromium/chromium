// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_metrics.h"

#include "base/metrics/histogram_macros.h"

void RecordJoinSetAction(KAnonymityServiceJoinSetAction action) {
  UMA_HISTOGRAM_ENUMERATION("Chrome.KAnonymityService.JoinSet.Action", action);
}

void RecordQuerySetAction(KAnonymityServiceQuerySetAction action) {
  UMA_HISTOGRAM_ENUMERATION("Chrome.KAnonymityService.QuerySet.Action", action);
}

void RecordQuerySetSize(size_t size) {
  UMA_HISTOGRAM_COUNTS_1000("Chrome.KAnonymityService.QuerySet.Size", size);
}

void RecordTrustTokenGetterAction(KAnonymityTrustTokenGetterAction action) {
  UMA_HISTOGRAM_ENUMERATION("Chrome.KAnonymityService.TrustTokenGetter.Action",
                            action);
}

void RecordTrustTokenGet(base::Time request_start, base::Time request_end) {
  UMA_HISTOGRAM_TIMES("Chrome.KAnonymityService.TrustTokenGetter.Latency",
                      request_end - request_start);
}
