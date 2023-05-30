// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_ORIGIN_ID_MANAGER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_ORIGIN_ID_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class MediaDrmOriginIdManager;
class Profile;

// Singleton that owns all MediaDrmOriginIdManagers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated MediaDrmOriginIdManager.
class MediaDrmOriginIdManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // This may return NULL if origin IDs are not supported by the profile
  // (e.g. incognito).
  static MediaDrmOriginIdManager* GetForProfile(Profile* profile);

  static MediaDrmOriginIdManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<MediaDrmOriginIdManagerFactory>;

  MediaDrmOriginIdManagerFactory();

  ~MediaDrmOriginIdManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_ORIGIN_ID_MANAGER_FACTORY_H_
