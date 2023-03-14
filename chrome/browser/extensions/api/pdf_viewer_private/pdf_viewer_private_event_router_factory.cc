// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_event_router_factory.h"

#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_event_router.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
PdfViewerPrivateEventRouter* PdfViewerPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<PdfViewerPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PdfViewerPrivateEventRouterFactory*
PdfViewerPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<PdfViewerPrivateEventRouterFactory>::get();
}

PdfViewerPrivateEventRouterFactory::PdfViewerPrivateEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "PdfViewerPrivateEventRouter",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(EventRouterFactory::GetInstance());
}

PdfViewerPrivateEventRouterFactory::~PdfViewerPrivateEventRouterFactory() =
    default;

KeyedService* PdfViewerPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return PdfViewerPrivateEventRouter::Create(context);
}

bool PdfViewerPrivateEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool PdfViewerPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
