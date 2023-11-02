// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_EXTENSIONS_HELPER_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_EXTENSIONS_HELPER_H_

#include "components/metrics/content/extensions_helper.h"

class ChromeMetricsExtensionsHelper : public metrics::ExtensionsHelper {
 public:
  ChromeMetricsExtensionsHelper();
  ChromeMetricsExtensionsHelper(const ChromeMetricsExtensionsHelper&) = delete;
  ChromeMetricsExtensionsHelper& operator=(
      const ChromeMetricsExtensionsHelper&) = delete;
  ~ChromeMetricsExtensionsHelper() override;

  // metrics::ExtensionsHelper:
  bool IsExtensionProcess(
      content::RenderProcessHost* render_process_host) override;
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_EXTENSIONS_HELPER_H_
