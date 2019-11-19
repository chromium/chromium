// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"

class ChromeSSLHostStateDelegate;
class Profile;

// Singleton that associates all ChromeSSLHostStateDelegates with
// Profiles.
class ChromeSSLHostStateDelegateFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeSSLHostStateDelegate* GetForProfile(Profile* profile);

  static ChromeSSLHostStateDelegateFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ChromeSSLHostStateDelegateFactory>;

  ChromeSSLHostStateDelegateFactory();
  ~ChromeSSLHostStateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ChromeSSLHostStateDelegateFactory);
};

#endif  // CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_FACTORY_H_
