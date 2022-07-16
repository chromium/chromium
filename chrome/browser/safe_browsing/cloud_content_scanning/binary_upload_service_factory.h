// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace safe_browsing {
class BinaryUploadService;

// Singleton that owns BinaryUploadService objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// service. It returns a separate instance if the profile is in the Incognito
// mode.
class BinaryUploadServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static BinaryUploadService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static BinaryUploadServiceFactory* GetInstance();

  BinaryUploadServiceFactory(const BinaryUploadServiceFactory&) = delete;
  BinaryUploadServiceFactory& operator=(const BinaryUploadServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<BinaryUploadServiceFactory>;

  BinaryUploadServiceFactory();
  ~BinaryUploadServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_FACTORY_H_
