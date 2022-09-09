// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_PREFS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_PREFS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace apps {

class SupportedLinksInfoBarPrefsService;

class SupportedLinksInfoBarPrefsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static SupportedLinksInfoBarPrefsService* GetForProfile(Profile* profile);

  static SupportedLinksInfoBarPrefsServiceFactory* GetInstance();
  SupportedLinksInfoBarPrefsServiceFactory(
      const SupportedLinksInfoBarPrefsServiceFactory&) = delete;
  SupportedLinksInfoBarPrefsServiceFactory& operator=(
      const SupportedLinksInfoBarPrefsServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      SupportedLinksInfoBarPrefsServiceFactory>;

  SupportedLinksInfoBarPrefsServiceFactory();
  ~SupportedLinksInfoBarPrefsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  // Service needs to be running immediately in order to observe uninstalls.
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_PREFS_SERVICE_FACTORY_H_
