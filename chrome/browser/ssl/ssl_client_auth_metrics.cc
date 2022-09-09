// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_metrics.h"

#include "base/metrics/histogram_macros.h"

void LogClientAuthResult(ClientCertSelectionResult result) {
  UMA_HISTOGRAM_ENUMERATION(kClientCertSelectHistogramName, result);
}
