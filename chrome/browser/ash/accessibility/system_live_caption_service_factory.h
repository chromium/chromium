// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SYSTEM_LIVE_CAPTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SYSTEM_LIVE_CAPTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace ash {

class SystemLiveCaptionService;

// Factory to get or create an instance of SystemLiveCaptionService for a
// Profile.
class SystemLiveCaptionServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static SystemLiveCaptionService* GetForProfile(Profile* profile);

  static SystemLiveCaptionServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<SystemLiveCaptionServiceFactory>;

  SystemLiveCaptionServiceFactory();
  ~SystemLiveCaptionServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SYSTEM_LIVE_CAPTION_SERVICE_FACTORY_H_
