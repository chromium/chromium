// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_SSL_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/prefs/pref_service.h"

class StatefulSSLHostStateDelegate;
class Profile;

// Singleton that associates all StatefulSSLHostStateDelegates with
// Profiles.
class StatefulSSLHostStateDelegateFactory : public ProfileKeyedServiceFactory {
 public:
  static StatefulSSLHostStateDelegate* GetForProfile(Profile* profile);

  static StatefulSSLHostStateDelegateFactory* GetInstance();

  StatefulSSLHostStateDelegateFactory(
      const StatefulSSLHostStateDelegateFactory&) = delete;
  StatefulSSLHostStateDelegateFactory& operator=(
      const StatefulSSLHostStateDelegateFactory&) = delete;

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactoryForTesting();

 private:
  friend base::NoDestructor<StatefulSSLHostStateDelegateFactory>;

  StatefulSSLHostStateDelegateFactory();
  ~StatefulSSLHostStateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SSL_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_
