// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace safe_browsing {
class BinaryUploadService;

// Singleton that owns CloudBinaryUploadService objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// service. It returns a separate instance if the profile is in the Incognito
// mode.
class CloudBinaryUploadServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static BinaryUploadService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static CloudBinaryUploadServiceFactory* GetInstance();

  CloudBinaryUploadServiceFactory(const CloudBinaryUploadServiceFactory&) =
      delete;
  CloudBinaryUploadServiceFactory& operator=(
      const CloudBinaryUploadServiceFactory&) = delete;

 private:
  friend base::NoDestructor<CloudBinaryUploadServiceFactory>;

  CloudBinaryUploadServiceFactory();
  ~CloudBinaryUploadServiceFactory() override = default;

  // CloudBrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_FACTORY_H_
