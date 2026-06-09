// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_TIPS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TIPS_TIPS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace tips {
class TipsService;

// Service factory to provide `TipsService` for a given profile.
class TipsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TipsServiceFactory* GetInstance();
  static TipsService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<TipsServiceFactory>;

  TipsServiceFactory();
  ~TipsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_TIPS_SERVICE_FACTORY_H_
