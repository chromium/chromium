// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_RANKER_FACTORY_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_RANKER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace translate {

class TranslateRanker;

class TranslateRankerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static TranslateRankerFactory* GetInstance();
  static translate::TranslateRanker* GetForBrowserContext(
      content::BrowserContext* browser_context);

  TranslateRankerFactory(const TranslateRankerFactory&) = delete;
  TranslateRankerFactory& operator=(const TranslateRankerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<TranslateRankerFactory>;

  TranslateRankerFactory();
  ~TranslateRankerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace translate

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_RANKER_FACTORY_H_
