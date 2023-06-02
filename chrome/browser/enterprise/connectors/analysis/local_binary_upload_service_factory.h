// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace safe_browsing {
class BinaryUploadService;
}

namespace enterprise_connectors {

// Singleton that owns LocalBinaryUploadService objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// service. It returns a separate instance if the profile is in the Incognito
// mode.
class LocalBinaryUploadServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static safe_browsing::BinaryUploadService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static LocalBinaryUploadServiceFactory* GetInstance();

  LocalBinaryUploadServiceFactory(const LocalBinaryUploadServiceFactory&) =
      delete;
  LocalBinaryUploadServiceFactory& operator=(
      const LocalBinaryUploadServiceFactory&) = delete;

 private:
  friend base::NoDestructor<LocalBinaryUploadServiceFactory>;

  LocalBinaryUploadServiceFactory();
  ~LocalBinaryUploadServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_FACTORY_H_
