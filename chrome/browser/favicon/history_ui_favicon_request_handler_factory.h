// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_HISTORY_UI_FAVICON_REQUEST_HANDLER_FACTORY_H_
#define CHROME_BROWSER_FAVICON_HISTORY_UI_FAVICON_REQUEST_HANDLER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace favicon {
class HistoryUiFaviconRequestHandler;
}

class HistoryUiFaviconRequestHandlerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static favicon::HistoryUiFaviconRequestHandler* GetForBrowserContext(
      content::BrowserContext* context);

  static HistoryUiFaviconRequestHandlerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      HistoryUiFaviconRequestHandlerFactory>;

  HistoryUiFaviconRequestHandlerFactory();
  ~HistoryUiFaviconRequestHandlerFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(HistoryUiFaviconRequestHandlerFactory);
};

#endif  // CHROME_BROWSER_FAVICON_HISTORY_UI_FAVICON_REQUEST_HANDLER_FACTORY_H_
