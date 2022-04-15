// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

// static
DataReductionProxyChromeSettings*
DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DataReductionProxyChromeSettings*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
bool DataReductionProxyChromeSettingsFactory::
    HasDataReductionProxyChromeSettings(content::BrowserContext* context) {
  return GetInstance()->GetServiceForBrowserContext(context, false) != NULL;
}

// static
DataReductionProxyChromeSettingsFactory*
DataReductionProxyChromeSettingsFactory::GetInstance() {
  return base::Singleton<DataReductionProxyChromeSettingsFactory>::get();
}

DataReductionProxyChromeSettingsFactory::
    DataReductionProxyChromeSettingsFactory()
    : BrowserContextKeyedServiceFactory(
          "DataReductionProxyChromeSettings",
          BrowserContextDependencyManager::GetInstance()) {}

DataReductionProxyChromeSettingsFactory::
    ~DataReductionProxyChromeSettingsFactory() {}

KeyedService* DataReductionProxyChromeSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());
  return new DataReductionProxyChromeSettings(context->IsOffTheRecord());
}
