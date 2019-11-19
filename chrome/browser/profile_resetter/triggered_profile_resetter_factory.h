// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_FACTORY_H_
#define CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class TriggeredProfileResetter;

class TriggeredProfileResetterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static TriggeredProfileResetter* GetForBrowserContext(
      content::BrowserContext* context);
  static TriggeredProfileResetterFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<TriggeredProfileResetterFactory>;
  friend class TriggeredProfileResetterTest;

  TriggeredProfileResetterFactory();
  ~TriggeredProfileResetterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(TriggeredProfileResetterFactory);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_FACTORY_H_
