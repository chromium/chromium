// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class PdfViewerPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the pdfViewerPrivate event router per profile (since the
// extension event router is per profile).
class PdfViewerPrivateEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  PdfViewerPrivateEventRouterFactory(
      const PdfViewerPrivateEventRouterFactory&) = delete;
  PdfViewerPrivateEventRouterFactory& operator=(
      const PdfViewerPrivateEventRouterFactory&) = delete;

  // Returns the PdfViewerPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static PdfViewerPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the PdfViewerPrivateEventRouterFactory instance.
  static PdfViewerPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend base::NoDestructor<PdfViewerPrivateEventRouterFactory>;

  PdfViewerPrivateEventRouterFactory();
  ~PdfViewerPrivateEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_EVENT_ROUTER_FACTORY_H_
