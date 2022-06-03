// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/chrome_content_browser_client_performance_manager_part.h"

#include "components/performance_manager/embedder/performance_manager_registry.h"

ChromeContentBrowserClientPerformanceManagerPart::
    ChromeContentBrowserClientPerformanceManagerPart() = default;
ChromeContentBrowserClientPerformanceManagerPart::
    ~ChromeContentBrowserClientPerformanceManagerPart() = default;

void ChromeContentBrowserClientPerformanceManagerPart::
    ExposeInterfacesToRenderer(
        service_manager::BinderRegistry* registry,
        blink::AssociatedInterfaceRegistry* associated_registry_unusued,
        content::RenderProcessHost* render_process_host) {
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->CreateProcessNodeAndExposeInterfacesToRendererProcess(
          registry, render_process_host);
}
