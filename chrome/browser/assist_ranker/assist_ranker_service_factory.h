// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASSIST_RANKER_ASSIST_RANKER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASSIST_RANKER_ASSIST_RANKER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace assist_ranker {
class AssistRankerService;
}

namespace assist_ranker {

class AssistRankerServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AssistRankerServiceFactory* GetInstance();
  static AssistRankerService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  AssistRankerServiceFactory(const AssistRankerServiceFactory&) = delete;
  AssistRankerServiceFactory& operator=(const AssistRankerServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<AssistRankerServiceFactory>;

  AssistRankerServiceFactory();
  ~AssistRankerServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace assist_ranker

#endif  // CHROME_BROWSER_ASSIST_RANKER_ASSIST_RANKER_SERVICE_FACTORY_H_
