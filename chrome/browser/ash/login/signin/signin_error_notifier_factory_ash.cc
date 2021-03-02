// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/signin_error_notifier_factory_ash.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

SigninErrorNotifierFactory::SigninErrorNotifierFactory()
    : BrowserContextKeyedServiceFactory(
          "SigninErrorNotifier",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninErrorControllerFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

SigninErrorNotifierFactory::~SigninErrorNotifierFactory() {}

// static
SigninErrorNotifier* SigninErrorNotifierFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninErrorNotifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninErrorNotifierFactory* SigninErrorNotifierFactory::GetInstance() {
  return base::Singleton<SigninErrorNotifierFactory>::get();
}

KeyedService* SigninErrorNotifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // If this is during dummy login from tests, suppress the notification.
  if (chromeos::switches::IsGaiaServicesDisabled())
    return nullptr;

  Profile* profile = static_cast<Profile*>(context);
  return new SigninErrorNotifier(
      SigninErrorControllerFactory::GetForProfile(profile), profile);
}
