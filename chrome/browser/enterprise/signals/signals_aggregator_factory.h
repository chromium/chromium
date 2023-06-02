// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_AGGREGATOR_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_AGGREGATOR_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace device_signals {
class SignalsAggregator;
}

namespace enterprise_signals {

// Singleton that owns a single SignalsAggregator instance.
class SignalsAggregatorFactory : public ProfileKeyedServiceFactory {
 public:
  static SignalsAggregatorFactory* GetInstance();
  static device_signals::SignalsAggregator* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<SignalsAggregatorFactory>;

  SignalsAggregatorFactory();
  ~SignalsAggregatorFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_AGGREGATOR_FACTORY_H_
