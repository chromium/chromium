// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOCKLIST_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_BLOCKLIST_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class Blocklist;

class BlocklistFactory : public ProfileKeyedServiceFactory {
 public:
  static Blocklist* GetForBrowserContext(content::BrowserContext* context);

  BlocklistFactory(const BlocklistFactory&) = delete;
  BlocklistFactory& operator=(const BlocklistFactory&) = delete;

  static BlocklistFactory* GetInstance();

 private:
  friend base::NoDestructor<BlocklistFactory>;

  BlocklistFactory();
  ~BlocklistFactory() override;

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLOCKLIST_FACTORY_H_
