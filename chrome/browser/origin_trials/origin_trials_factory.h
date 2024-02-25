// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_FACTORY_H_
#define CHROME_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_FACTORY_H_

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/origin_trials_controller_delegate.h"

namespace content {
class BrowserContext;
}

class OriginTrialsFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of OriginTrialsControllerDelegate associated with
  // |context| or nullptr if persistent origin trials are disabled or the
  // context in question doesn't support persistent origin trials - for example
  // if it is the system profile.
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
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_FACTORY_H_
