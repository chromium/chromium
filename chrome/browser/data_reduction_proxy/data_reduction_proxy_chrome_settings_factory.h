// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_FACTORY_H_
#define CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class DataReductionProxyChromeSettings;

// Constucts a DataReductionProxySettings object suitable for use with a
// Chrome browser.
class DataReductionProxyChromeSettingsFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns a settings object for the given context.
  static DataReductionProxyChromeSettings* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns true if this context has a settings object.
  static bool HasDataReductionProxyChromeSettings(
      content::BrowserContext* context);

  // Returns an instance of this factory.
  static DataReductionProxyChromeSettingsFactory* GetInstance();

  DataReductionProxyChromeSettingsFactory(
      const DataReductionProxyChromeSettingsFactory&) = delete;
  DataReductionProxyChromeSettingsFactory& operator=(
      const DataReductionProxyChromeSettingsFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      DataReductionProxyChromeSettingsFactory>;

  DataReductionProxyChromeSettingsFactory();

  ~DataReductionProxyChromeSettingsFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_FACTORY_H_
