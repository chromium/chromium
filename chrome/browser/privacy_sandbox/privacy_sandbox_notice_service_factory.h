// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

class PrivacySandboxNoticeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PrivacySandboxNoticeServiceFactory* GetInstance();
  static privacy_sandbox::PrivacySandboxNoticeService* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<PrivacySandboxNoticeServiceFactory>;
  PrivacySandboxNoticeServiceFactory();
  ~PrivacySandboxNoticeServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_SERVICE_FACTORY_H_
