// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_DISTILLER_DOM_DISTILLER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOM_DISTILLER_DOM_DISTILLER_SERVICE_FACTORY_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller_ui_handle.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace dom_distiller {

// A simple wrapper for DomDistillerService to expose it as a
// KeyedService.
class DomDistillerContextKeyedService : public KeyedService,
                                        public DomDistillerService {
 public:
  DomDistillerContextKeyedService(
      std::unique_ptr<DistillerFactory> distiller_factory,
      std::unique_ptr<DistillerPageFactory> distiller_page_factory,
      std::unique_ptr<DistilledPagePrefs> distilled_page_prefs,
      std::unique_ptr<DistillerUIHandle> distiller_ui_handle,
      base::CallbackListSubscription distilled_page_prefs_subscription);

  DomDistillerContextKeyedService(const DomDistillerContextKeyedService&) =
      delete;
  DomDistillerContextKeyedService& operator=(
      const DomDistillerContextKeyedService&) = delete;

  ~DomDistillerContextKeyedService() override;

 private:
  base::CallbackListSubscription distilled_page_prefs_subscription_;
};

class DomDistillerServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DomDistillerServiceFactory* GetInstance();
  static DomDistillerContextKeyedService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<DomDistillerServiceFactory>;

  DomDistillerServiceFactory();
  ~DomDistillerServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void UpdateDistilledPagePrefsDefaultFontScaling(
      content::BrowserContext* context) const;

  base::WeakPtrFactory<DomDistillerServiceFactory> weak_ptr_factory_{this};
};

}  // namespace dom_distiller

#endif  // CHROME_BROWSER_DOM_DISTILLER_DOM_DISTILLER_SERVICE_FACTORY_H_
