// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_THROTTLE_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_THROTTLE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crostini {

class CrostiniThrottle;

class CrostiniThrottleFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use Crostini.
  static CrostiniThrottle* GetForBrowserContext(
      content::BrowserContext* context);

  static CrostiniThrottleFactory* GetInstance();

  CrostiniThrottleFactory(const CrostiniThrottleFactory&) = delete;
  CrostiniThrottleFactory& operator=(const CrostiniThrottleFactory&) = delete;

 private:
  friend class base::NoDestructor<CrostiniThrottleFactory>;

  CrostiniThrottleFactory();
  ~CrostiniThrottleFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_THROTTLE_FACTORY_H_
