// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class LiteVideoKeyedService;
class Profile;

// LazyInstance that owns all LiteVideoKeyedServices and associates them
// with Profiles.
class LiteVideoKeyedServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the LiteVideoService for the profile.
  //
  // Returns null if the features that allow for this to provide useful
  // information are disabled.
  static LiteVideoKeyedService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all LiteVideoKeyedService(s).
  static LiteVideoKeyedServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<LiteVideoKeyedServiceFactory>;

  LiteVideoKeyedServiceFactory();
  ~LiteVideoKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  //  CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_KEYED_SERVICE_FACTORY_H_
