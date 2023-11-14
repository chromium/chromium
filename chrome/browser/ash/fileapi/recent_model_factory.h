// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_FACTORY_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {

class RecentModel;

class RecentModelFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the RecentModel for |profile|, creating it if not created yet.
  static RecentModel* GetForProfile(Profile* profile);

  // Returns the singleton RecentModelFactory instance.
  static RecentModelFactory* GetInstance();

  RecentModelFactory(const RecentModelFactory&) = delete;
  RecentModelFactory& operator=(const RecentModelFactory&) = delete;

 private:
  friend base::NoDestructor<RecentModelFactory>;

  RecentModelFactory();
  ~RecentModelFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_FACTORY_H_
