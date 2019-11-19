// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service_factory.h"

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
IntentPickerAutoDisplayService*
IntentPickerAutoDisplayServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<IntentPickerAutoDisplayService*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 /*create_if_necessary=*/true));
}

// static
IntentPickerAutoDisplayServiceFactory*
IntentPickerAutoDisplayServiceFactory::GetInstance() {
  return base::Singleton<IntentPickerAutoDisplayServiceFactory>::get();
}

IntentPickerAutoDisplayServiceFactory::IntentPickerAutoDisplayServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "IntentPickerAutoDisplayService",
          BrowserContextDependencyManager::GetInstance()) {}

IntentPickerAutoDisplayServiceFactory::
    ~IntentPickerAutoDisplayServiceFactory() = default;

KeyedService* IntentPickerAutoDisplayServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new IntentPickerAutoDisplayService(
      Profile::FromBrowserContext(context));
}
