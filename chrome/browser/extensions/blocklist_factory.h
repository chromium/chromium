// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOCKLIST_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_BLOCKLIST_FACTORY_H_

#include "base/memory/singleton.h"
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
  friend struct base::DefaultSingletonTraits<BlocklistFactory>;

  BlocklistFactory();
  ~BlocklistFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLOCKLIST_FACTORY_H_
