// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/android_sms/android_sms_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ash {
namespace android_sms {

class AndroidSmsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AndroidSmsServiceFactory* GetInstance();

  static AndroidSmsService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  AndroidSmsServiceFactory(const AndroidSmsServiceFactory&) = delete;
  AndroidSmsServiceFactory& operator=(const AndroidSmsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AndroidSmsServiceFactory>;

  AndroidSmsServiceFactory();
  ~AndroidSmsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace android_sms
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SERVICE_FACTORY_H_
