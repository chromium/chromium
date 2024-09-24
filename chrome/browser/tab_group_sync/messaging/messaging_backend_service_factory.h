// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_GROUP_SYNC_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TAB_GROUP_SYNC_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/saved_tab_groups/messaging/messaging_backend_service.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace tab_groups::messaging {
class MessagingBackendService;

// A factory to create a unique MessagingBackendService.
class MessagingBackendServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the MessagingBackendService for the profile. Returns null for
  // incognito.
  // The caller is responsible for checking that the
  // data_sharing::features::kDataSharingFeature is enabled.
  static MessagingBackendService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of MessagingBackendServiceFactory.
  static MessagingBackendServiceFactory* GetInstance();

  // Disallow copy/assign.
  MessagingBackendServiceFactory(const MessagingBackendServiceFactory&) =
      delete;
  void operator=(const MessagingBackendServiceFactory&) = delete;

 private:
  friend base::NoDestructor<MessagingBackendServiceFactory>;

  MessagingBackendServiceFactory();
  ~MessagingBackendServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace tab_groups::messaging

#endif  // CHROME_BROWSER_TAB_GROUP_SYNC_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_
