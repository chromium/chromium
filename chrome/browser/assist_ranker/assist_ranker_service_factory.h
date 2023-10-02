// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASSIST_RANKER_ASSIST_RANKER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASSIST_RANKER_ASSIST_RANKER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace assist_ranker {
class AssistRankerService;
}

namespace assist_ranker {

class AssistRankerServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AssistRankerServiceFactory* GetInstance();
  static AssistRankerService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  AssistRankerServiceFactory(const AssistRankerServiceFactory&) = delete;
  AssistRankerServiceFactory& operator=(const AssistRankerServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<AssistRankerServiceFactory>;

  AssistRankerServiceFactory();
  ~AssistRankerServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace assist_ranker

#endif  // CHROME_BROWSER_ASSIST_RANKER_ASSIST_RANKER_SERVICE_FACTORY_H_
