// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DATA_SHARING_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace data_sharing::personal_collaboration_data {
class PersonalCollaborationDataService;

// A factory to create a unique PersonalCollaborationDataService.
class PersonalCollaborationDataServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Gets the PersonalCollaborationDataService for the profile. Returns null for
  // incognito/guest.
  static PersonalCollaborationDataService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of
  // PersonalCollaborationDataServiceFactory.
  static PersonalCollaborationDataServiceFactory* GetInstance();

  // Disallow copy/assign.
  PersonalCollaborationDataServiceFactory(
      const PersonalCollaborationDataServiceFactory&) = delete;
  PersonalCollaborationDataServiceFactory& operator=(
      const PersonalCollaborationDataServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PersonalCollaborationDataServiceFactory>;

  PersonalCollaborationDataServiceFactory();
  ~PersonalCollaborationDataServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace data_sharing::personal_collaboration_data

#endif  // CHROME_BROWSER_DATA_SHARING_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_
