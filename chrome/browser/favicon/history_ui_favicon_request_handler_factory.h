// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_HISTORY_UI_FAVICON_REQUEST_HANDLER_FACTORY_H_
#define CHROME_BROWSER_FAVICON_HISTORY_UI_FAVICON_REQUEST_HANDLER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

namespace favicon {
class HistoryUiFaviconRequestHandler;
}

class HistoryUiFaviconRequestHandlerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static favicon::HistoryUiFaviconRequestHandler* GetForBrowserContext(
      content::BrowserContext* context);

  static HistoryUiFaviconRequestHandlerFactory* GetInstance();

  HistoryUiFaviconRequestHandlerFactory(
      const HistoryUiFaviconRequestHandlerFactory&) = delete;
  HistoryUiFaviconRequestHandlerFactory& operator=(
      const HistoryUiFaviconRequestHandlerFactory&) = delete;

 private:
  friend base::NoDestructor<HistoryUiFaviconRequestHandlerFactory>;

  HistoryUiFaviconRequestHandlerFactory();
  ~HistoryUiFaviconRequestHandlerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_FAVICON_HISTORY_UI_FAVICON_REQUEST_HANDLER_FACTORY_H_
