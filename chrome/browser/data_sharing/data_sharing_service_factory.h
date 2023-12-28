// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_DATA_SHARING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DATA_SHARING_DATA_SHARING_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace data_sharing {
class DataSharingService;

// A factory to create a unique DataSharingService.
class DataSharingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the DataSharingService for the profile. Returns a non-null, but empty
  // implementation if the feature isn't enabled.
  static DataSharingService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of DataSharingServiceFactory.
  static DataSharingServiceFactory* GetInstance();

  // Disallow copy/assign.
  DataSharingServiceFactory(const DataSharingServiceFactory&) = delete;
  DataSharingServiceFactory& operator=(const DataSharingServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<DataSharingServiceFactory>;

  DataSharingServiceFactory();
  ~DataSharingServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_DATA_SHARING_SERVICE_FACTORY_H_
