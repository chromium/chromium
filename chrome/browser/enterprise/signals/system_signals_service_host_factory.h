// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_SYSTEM_SIGNALS_SERVICE_HOST_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_SYSTEM_SIGNALS_SERVICE_HOST_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace device_signals {
class SystemSignalsServiceHost;
}

namespace enterprise_signals {

// Singleton that owns a single SystemSignalsServiceHost instance.
class SystemSignalsServiceHostFactory : public ProfileKeyedServiceFactory {
 public:
  static SystemSignalsServiceHostFactory* GetInstance();
  static device_signals::SystemSignalsServiceHost* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<SystemSignalsServiceHostFactory>;

  SystemSignalsServiceHostFactory();
  ~SystemSignalsServiceHostFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_SYSTEM_SIGNALS_SERVICE_HOST_FACTORY_H_
