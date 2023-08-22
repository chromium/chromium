// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace captions {

class LiveCaptionController;

// Factory to get or create an instance of LiveCaptionController from a Profile.
class LiveCaptionControllerFactory : public ProfileKeyedServiceFactory {
 public:
  static LiveCaptionController* GetForProfile(Profile* profile);

  static LiveCaptionController* GetForProfileIfExists(Profile* profile);

  static LiveCaptionControllerFactory* GetInstance();

 private:
  friend base::NoDestructor<LiveCaptionControllerFactory>;

  LiveCaptionControllerFactory();
  ~LiveCaptionControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_FACTORY_H_
