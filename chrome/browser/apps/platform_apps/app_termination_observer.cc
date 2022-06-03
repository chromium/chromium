// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_termination_observer.h"

#include "apps/browser_context_keyed_service_factories.h"
#include "base/no_destructor.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chrome_apps {

namespace {

class AppTerminationObserverFactory : public BrowserContextKeyedServiceFactory {
 public:
  AppTerminationObserverFactory();
  AppTerminationObserverFactory(const AppTerminationObserverFactory&) = delete;
  AppTerminationObserverFactory& operator=(
      const AppTerminationObserverFactory&) = delete;
  ~AppTerminationObserverFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

AppTerminationObserverFactory::AppTerminationObserverFactory()
    : BrowserContextKeyedServiceFactory(
          "AppTerminationObserver",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* AppTerminationObserverFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new AppTerminationObserver(browser_context);
}

content::BrowserContext* AppTerminationObserverFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  return Profile::FromBrowserContext(browser_context)->GetOriginalProfile();
}

bool AppTerminationObserverFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace

AppTerminationObserver::AppTerminationObserver(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

AppTerminationObserver::~AppTerminationObserver() = default;

// static
BrowserContextKeyedServiceFactory*
AppTerminationObserver::GetFactoryInstance() {
  static base::NoDestructor<AppTerminationObserverFactory> factory;
  return factory.get();
}

void AppTerminationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  // NOTE: This fires on application termination, but passes in an associated
  // BrowserContext. If a BrowserContext is actually destroyed *before*
  // application termination, we won't call NotifyApplicationTerminating() for
  // that context. We could instead monitor BrowserContext destruction if this
  // is an issue.
  apps::NotifyApplicationTerminating(browser_context_);
}

}  // namespace chrome_apps
