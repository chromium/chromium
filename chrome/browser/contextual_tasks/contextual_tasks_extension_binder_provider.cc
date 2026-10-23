// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_binder_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/plural_string_provider.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"
#endif

namespace contextual_tasks {

namespace {

// Routes `mojom::ExtensionPageHandlerFactory` requests from the extension's
// service worker (`background.ts`) to the profile-scoped
// `ContextualTasksExtensionServiceWorkerHandler`. Unlike extension iframes, the
// service worker has no `RenderFrameHost` and cannot bind to the
// document-scoped `ContextualTasksExtensionHandler`.
void BindServiceWorkerFactoryReceiver(
    content::BrowserContext* context,
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<mojom::ExtensionPageHandlerFactory> receiver) {
  auto* bridge =
      ContextualTasksExtensionBridge::Get(Profile::FromBrowserContext(context));
  if (bridge) {
    bridge->service_worker_handler().BindFactory(std::move(receiver));
  }
}

}  // namespace

// static
void ContextualTasksExtensionBinderProvider::Register(
    content::BrowserContext* browser_context) {
  extensions::ExtensionMojoBinderRegistryFactory::GetOrCreateForBrowserContext(
      browser_context)
      ->RegisterProvider(
          base::PassKey<ContextualTasksExtensionBinderProvider>(),
          std::make_unique<ContextualTasksExtensionBinderProvider>());
}

ContextualTasksExtensionBinderProvider::ContextualTasksExtensionBinderProvider()
    : ExtensionMojoBinderProvider(extension_misc::kContextualTasksExtensionId) {
}

ContextualTasksExtensionBinderProvider::
    ~ContextualTasksExtensionBinderProvider() = default;

bool ContextualTasksExtensionBinderProvider::IsMojoJsEnabledForFrame() const {
  return true;
}

bool ContextualTasksExtensionBinderProvider::IsMojoJsEnabledForServiceWorker()
    const {
  // Exposes the `Mojo` JS bindings object inside the extension service worker
  // execution context (`background.ts`), which is checked separately from
  // extension document frames (`IsMojoJsEnabledForFrame()`).
  return true;
}

void ContextualTasksExtensionBinderProvider::PopulateFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
    content::RenderFrameHost* render_frame_host,
    const extensions::Extension& extension) {
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksRearchitecture)) {
    binder_map.Add<contextual_tasks::mojom::ExtensionPageHandlerFactory>(
        base::BindRepeating(
            [](content::RenderFrameHost* frame_host,
               mojo::PendingReceiver<
                   contextual_tasks::mojom::ExtensionPageHandlerFactory>
                   receiver) {
              ::ContextualTasksExtensionHandler::GetOrCreateForCurrentDocument(
                  frame_host)
                  ->BindContextualTasksFactory(std::move(receiver));
            }));
    binder_map.Add<tracked_element::mojom::TrackedElementHandler>(
        base::BindRepeating(
            [](content::RenderFrameHost* frame_host,
               mojo::PendingReceiver<
                   tracked_element::mojom::TrackedElementHandler> receiver) {
              ::ContextualTasksExtensionHandler::GetOrCreateForCurrentDocument(
                  frame_host)
                  ->BindTrackedElementHandler(std::move(receiver));
            }));
  }
  binder_map.Add<composebox::mojom::PageHandlerFactory>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<composebox::mojom::PageHandlerFactory>
             receiver) {
        ::ContextualTasksExtensionHandler::GetOrCreateForCurrentDocument(
            frame_host)
            ->BindComposeboxFactory(std::move(receiver));
      }));
  binder_map.Add<plural_string::mojom::PluralStringProvider>(
      base::BindRepeating(
          [](content::RenderFrameHost* frame_host,
             mojo::PendingReceiver<plural_string::mojom::PluralStringProvider>
                 receiver) {
            PluralStringProvider::Create(std::move(receiver),
                                         GetSearchboxPluralStringIds());
          }));
#endif
}

void ContextualTasksExtensionBinderProvider::PopulateServiceWorkerBinders(
    mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>&
        binder_map,
    content::BrowserContext* browser_context,
    const extensions::Extension& extension) {
  // Even though the service worker reuses `mojom::ExtensionPageHandlerFactory`,
  // service worker `BrowserInterfaceBroker` requests are dispatched here rather
  // than `PopulateFrameBinders()` and must be bound to the profile-scoped
  // `ContextualTasksExtensionServiceWorkerHandler`.
  if (base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksRearchitecture)) {
    binder_map.Add<mojom::ExtensionPageHandlerFactory>(base::BindRepeating(
        &BindServiceWorkerFactoryReceiver, browser_context));
  }
}

}  // namespace contextual_tasks
