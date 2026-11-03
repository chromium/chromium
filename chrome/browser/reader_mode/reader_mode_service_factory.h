// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READER_MODE_READER_MODE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_READER_MODE_READER_MODE_SERVICE_FACTORY_H_

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

namespace reader_mode {

// A simple wrapper for the distiller service to expose it as a KeyedService.
class ReaderModeContextKeyedService
    : public KeyedService,
      public dom_distiller::DomDistillerService {
 public:
  ReaderModeContextKeyedService(
      std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory,
      std::unique_ptr<dom_distiller::DistillerPageFactory>
          distiller_page_factory,
      std::unique_ptr<dom_distiller::DistilledPagePrefs> distilled_page_prefs,
      std::unique_ptr<dom_distiller::DistillerUIHandle> distiller_ui_handle,
      base::CallbackListSubscription distilled_page_prefs_subscription);

  ReaderModeContextKeyedService(const ReaderModeContextKeyedService&) = delete;
  ReaderModeContextKeyedService& operator=(
      const ReaderModeContextKeyedService&) = delete;

  ~ReaderModeContextKeyedService() override;

 private:
  base::CallbackListSubscription distilled_page_prefs_subscription_;
};

class ReaderModeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ReaderModeServiceFactory* GetInstance();
  static ReaderModeContextKeyedService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<ReaderModeServiceFactory>;

  ReaderModeServiceFactory();
  ~ReaderModeServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void UpdateDistilledPagePrefsDefaultFontScaling(
      content::BrowserContext* context) const;

  base::WeakPtrFactory<ReaderModeServiceFactory> weak_ptr_factory_{this};
};

}  // namespace reader_mode

#endif  // CHROME_BROWSER_READER_MODE_READER_MODE_SERVICE_FACTORY_H_
