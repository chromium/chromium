// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/chrome_content_browser_client_performance_manager_part.h"

#include "components/performance_manager/embedder/binders.h"
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
  auto* pm_registry =
      performance_manager::PerformanceManagerRegistry::GetInstance();
  if (pm_registry) {
    pm_registry->CreateProcessNode(render_process_host);
    pm_registry->GetBinders().ExposeInterfacesToRendererProcess(
        registry, render_process_host);
  }
}
