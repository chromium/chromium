// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_extensions_helper.h"

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
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
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(
              render_process_host->GetBrowserContext())) {
    return false;
  }

  auto* process_map =
      extensions::ProcessMap::Get(render_process_host->GetBrowserContext());
  CHECK(process_map);
  return process_map->Contains(render_process_host->GetID());
#else
  return false;
#endif
}
