// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_FACTORY_H_
#define CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/client_hints_controller_delegate.h"

namespace content {
class BrowserContext;
}

class ClientHintsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static content::ClientHintsControllerDelegate* GetForBrowserContext(
      content::BrowserContext* context);

  static ClientHintsFactory* GetInstance();

  ClientHintsFactory(const ClientHintsFactory&) = delete;
  ClientHintsFactory& operator=(const ClientHintsFactory&) = delete;

 private:
  friend struct base::LazyInstanceTraitsBase<ClientHintsFactory>;

  ClientHintsFactory();
  ~ClientHintsFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_FACTORY_H_
