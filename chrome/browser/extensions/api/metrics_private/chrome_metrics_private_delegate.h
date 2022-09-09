// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_CHROME_METRICS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_CHROME_METRICS_PRIVATE_DELEGATE_H_

#include "extensions/browser/api/metrics_private/metrics_private_delegate.h"

namespace extensions {

class ChromeMetricsPrivateDelegate : public MetricsPrivateDelegate {
 public:
  ChromeMetricsPrivateDelegate() {}

  ChromeMetricsPrivateDelegate(const ChromeMetricsPrivateDelegate&) = delete;
  ChromeMetricsPrivateDelegate& operator=(const ChromeMetricsPrivateDelegate&) =
      delete;

  ~ChromeMetricsPrivateDelegate() override {}

  // MetricsPrivateDelegate:
  bool IsCrashReportingEnabled() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_CHROME_METRICS_PRIVATE_DELEGATE_H_
