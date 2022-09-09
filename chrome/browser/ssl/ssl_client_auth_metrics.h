// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_METRICS_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_METRICS_H_

const char kClientCertSelectHistogramName[] =
    "Security.ClientAuth.CertificateSelectionSource";

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "ClientCertSelectionResult" in src/tools/metrics/histograms/enums.xml.
enum class ClientCertSelectionResult {
  kUserSelect = 0,
  kUserCancel = 1,
  kUserCloseTab = 2,
  kAutoSelect = 3,
  kNoSelectionAllowed = 4,
  kMaxValue = kNoSelectionAllowed,
};

void LogClientAuthResult(ClientCertSelectionResult result);

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_METRICS_H_
