// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_extensions_helper.h"

#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

ChromeMetricsExtensionsHelper::ChromeMetricsExtensionsHelper() = default;
ChromeMetricsExtensionsHelper::~ChromeMetricsExtensionsHelper() = default;

bool ChromeMetricsExtensionsHelper::IsExtensionProcess(
    content::RenderProcessHost* render_process_host) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions::ProcessMap::Get(render_process_host->GetBrowserContext())
      ->Contains(render_process_host->GetID());
#else
  return false;
#endif
}
