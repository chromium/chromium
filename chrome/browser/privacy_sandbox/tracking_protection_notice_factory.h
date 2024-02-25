// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_FACTORY_H_

#include <memory>
#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_notice_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

class TrackingProtectionNoticeFactory : public ProfileKeyedServiceFactory {
 public:
  static TrackingProtectionNoticeFactory* GetInstance();
  static privacy_sandbox::TrackingProtectionNoticeService* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<TrackingProtectionNoticeFactory>;
  TrackingProtectionNoticeFactory();
  ~TrackingProtectionNoticeFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_FACTORY_H_
