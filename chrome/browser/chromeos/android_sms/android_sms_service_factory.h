// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace chromeos {

namespace android_sms {

class AndroidSmsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AndroidSmsServiceFactory* GetInstance();

  static AndroidSmsService* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend class base::NoDestructor<AndroidSmsServiceFactory>;

  AndroidSmsServiceFactory();
  ~AndroidSmsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsServiceFactory);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_FACTORY_H_
