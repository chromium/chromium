// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class HostContentSettingsMap;

class HostContentSettingsMapFactory
    : public RefcountedProfileKeyedServiceFactory {
 public:
  static HostContentSettingsMap* GetForProfile(
      content::BrowserContext* browser_context);
  static HostContentSettingsMapFactory* GetInstance();

  HostContentSettingsMapFactory(const HostContentSettingsMapFactory&) = delete;
  HostContentSettingsMapFactory& operator=(
      const HostContentSettingsMapFactory&) = delete;

 private:
  friend base::NoDestructor<HostContentSettingsMapFactory>;

  HostContentSettingsMapFactory();
  ~HostContentSettingsMapFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory methods:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif // CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
