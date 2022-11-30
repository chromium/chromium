// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_termination_observer.h"

#include "apps/browser_context_keyed_service_factories.h"
#include "base/no_destructor.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace chrome_apps {

namespace {

class AppTerminationObserverFactory : public ProfileKeyedServiceFactory {
 public:
  AppTerminationObserverFactory();
  AppTerminationObserverFactory(const AppTerminationObserverFactory&) = delete;
  AppTerminationObserverFactory& operator=(
      const AppTerminationObserverFactory&) = delete;
  ~AppTerminationObserverFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

AppTerminationObserverFactory::AppTerminationObserverFactory()
    : ProfileKeyedServiceFactory(
          "AppTerminationObserver",
          ProfileSelections::BuildRedirectedInIncognito()) {}

KeyedService* AppTerminationObserverFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new AppTerminationObserver(browser_context);
}

bool AppTerminationObserverFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace

AppTerminationObserver::AppTerminationObserver(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  // base::Unretained(this) is safe here as this object owns |subscription_| and
  // the callback won't be invoked after the subscription is destroyed.
  subscription_ = browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
      &AppTerminationObserver::OnAppTerminating, base::Unretained(this)));
}

AppTerminationObserver::~AppTerminationObserver() = default;

// static
BrowserContextKeyedServiceFactory*
AppTerminationObserver::GetFactoryInstance() {
  static base::NoDestructor<AppTerminationObserverFactory> factory;
  return factory.get();
}

void AppTerminationObserver::OnAppTerminating() {
  // NOTE: This fires on application termination, but passes in an associated
  // BrowserContext. If a BrowserContext is actually destroyed *before*
  // application termination, we won't call NotifyApplicationTerminating() for
  // that context. We could instead monitor BrowserContext destruction if this
  // is an issue.
  apps::NotifyApplicationTerminating(browser_context_);
}

}  // namespace chrome_apps
