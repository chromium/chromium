// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace send_tab_to_self {
class SendTabToSelfClientService;

// Singleton that owns the SendTabToSelfClientService and associates them with
// Profile.
class SendTabToSelfClientServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static send_tab_to_self::SendTabToSelfClientService* GetForProfile(
      Profile* profile);
  static SendTabToSelfClientServiceFactory* GetInstance();

  SendTabToSelfClientServiceFactory(const SendTabToSelfClientServiceFactory&) =
      delete;
  SendTabToSelfClientServiceFactory& operator=(
      const SendTabToSelfClientServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SendTabToSelfClientServiceFactory>;

  SendTabToSelfClientServiceFactory();
  ~SendTabToSelfClientServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_
