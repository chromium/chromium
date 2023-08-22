// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_LIVE_TRANSLATE_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_LIVE_TRANSLATE_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace captions {

class LiveTranslateController;

// Factory to get or create an instance of LiveTranslateController from a
// Profile.
class LiveTranslateControllerFactory : public ProfileKeyedServiceFactory {
 public:
  static LiveTranslateController* GetForProfile(Profile* profile);
  static LiveTranslateControllerFactory* GetInstance();

 private:
  friend base::NoDestructor<LiveTranslateControllerFactory>;

  LiveTranslateControllerFactory();
  ~LiveTranslateControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_LIVE_TRANSLATE_CONTROLLER_FACTORY_H_
