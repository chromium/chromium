// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COLLABORATION_COLLABORATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_COLLABORATION_COLLABORATION_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace collaboration {
class CollaborationService;

// A factory to create a unique CollaborationService.
class CollaborationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the CollaborationService for the profile.
  static CollaborationService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of CollaborationServiceFactory.
  static CollaborationServiceFactory* GetInstance();

  // Disallow copy/assign.
  CollaborationServiceFactory(const CollaborationServiceFactory&) = delete;
  void operator=(const CollaborationServiceFactory&) = delete;

 private:
  friend base::NoDestructor<CollaborationServiceFactory>;

  CollaborationServiceFactory();
  ~CollaborationServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace collaboration

#endif  // CHROME_BROWSER_COLLABORATION_COLLABORATION_SERVICE_FACTORY_H_
