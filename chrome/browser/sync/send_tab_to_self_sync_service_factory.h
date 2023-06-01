// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace send_tab_to_self {
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

class SendTabToSelfSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static send_tab_to_self::SendTabToSelfSyncService* GetForProfile(
      Profile* profile);
  static SendTabToSelfSyncServiceFactory* GetInstance();

  SendTabToSelfSyncServiceFactory(const SendTabToSelfSyncServiceFactory&) =
      delete;
  SendTabToSelfSyncServiceFactory& operator=(
      const SendTabToSelfSyncServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SendTabToSelfSyncServiceFactory>;

  SendTabToSelfSyncServiceFactory();
  ~SendTabToSelfSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
