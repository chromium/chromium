// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_ANDROID_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_METRICS_ANDROID_SESSION_DURATIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class AndroidSessionDurationsService;
class Profile;

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

class AndroidSessionDurationsServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given
  // BrowserContext.
  static AndroidSessionDurationsService* GetForProfile(Profile* profile);

  static void OnAppEnterForeground(base::TimeTicks session_start);
  static void OnAppEnterBackground(base::TimeDelta session_length);

  static AndroidSessionDurationsServiceFactory* GetInstance();

  AndroidSessionDurationsServiceFactory(
      const AndroidSessionDurationsServiceFactory&) = delete;
  AndroidSessionDurationsServiceFactory& operator=(
      const AndroidSessionDurationsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AndroidSessionDurationsServiceFactory>;
  AndroidSessionDurationsServiceFactory();
  ~AndroidSessionDurationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_ANDROID_SESSION_DURATIONS_SERVICE_FACTORY_H_
