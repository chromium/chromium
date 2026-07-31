// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_binder_provider.h"

#include <memory>

#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"
#include "extensions/common/constants.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"
#endif

namespace contextual_tasks {

// static
void ContextualTasksExtensionBinderProvider::Register(
    content::BrowserContext* browser_context) {
  extensions::ExtensionMojoBinderRegistryFactory::GetForBrowserContext(
      browser_context)
      ->RegisterProvider(
          base::PassKey<ContextualTasksExtensionBinderProvider>(),
          std::make_unique<ContextualTasksExtensionBinderProvider>());
}

ContextualTasksExtensionBinderProvider::
    ContextualTasksExtensionBinderProvider() = default;

ContextualTasksExtensionBinderProvider::
    ~ContextualTasksExtensionBinderProvider() = default;

extensions::ExtensionId ContextualTasksExtensionBinderProvider::GetExtensionId()
    const {
  return extension_misc::kContextualTasksExtensionId;
}

void ContextualTasksExtensionBinderProvider::PopulateFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
    content::RenderFrameHost* render_frame_host,
    const extensions::Extension* extension) {
#if !BUILDFLAG(IS_ANDROID)
  binder_map.Add<composebox::mojom::PageHandlerFactory>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<composebox::mojom::PageHandlerFactory>
             receiver) {
        ContextualTasksExtensionHandler::GetOrCreateForCurrentDocument(
            frame_host)
            ->BindComposeboxFactory(std::move(receiver));
      }));
#endif
}

}  // namespace contextual_tasks
