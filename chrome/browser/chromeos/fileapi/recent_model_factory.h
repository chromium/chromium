// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {

class RecentModel;

class RecentModelFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the RecentModel for |profile|, creating it if not created yet.
  static RecentModel* GetForProfile(Profile* profile);

  // Returns the singleton RecentModelFactory instance.
  static RecentModelFactory* GetInstance();

  RecentModelFactory(const RecentModelFactory&) = delete;
  RecentModelFactory& operator=(const RecentModelFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<RecentModelFactory>;

  RecentModelFactory();
  ~RecentModelFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_FACTORY_H_
