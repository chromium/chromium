// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_FACTORY_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_FACTORY_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

namespace content_settings {
class CookieSettings;
}

class Profile;

class CookieSettingsFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  // Returns the |CookieSettings| associated with the |profile|.
  //
  // This should only be called on the UI thread.
  static scoped_refptr<content_settings::CookieSettings> GetForProfile(
      Profile* profile);

  static CookieSettingsFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CookieSettingsFactory>;

  CookieSettingsFactory();
  ~CookieSettingsFactory() override;

  // |RefcountedBrowserContextKeyedServiceFactory| methods:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CookieSettingsFactory);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_FACTORY_H_
