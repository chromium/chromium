// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_FACTORY_H_
#define CHROME_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/origin_trials_controller_delegate.h"

namespace content {
class BrowserContext;
}

class OriginTrialsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static content::OriginTrialsControllerDelegate* GetForBrowserContext(
      content::BrowserContext* context);

  static OriginTrialsFactory* GetInstance();

  OriginTrialsFactory(const OriginTrialsFactory&) = delete;
  OriginTrialsFactory& operator=(const OriginTrialsFactory&) = delete;

 private:
  friend struct base::LazyInstanceTraitsBase<OriginTrialsFactory>;

  OriginTrialsFactory();
  ~OriginTrialsFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_FACTORY_H_
