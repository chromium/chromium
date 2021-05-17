// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_controller_factory.h"

#include "build/build_config.h"
#include "chrome/browser/accessibility/caption_controller.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace captions {

// static
CaptionController* CaptionControllerFactory::GetForProfile(Profile* profile) {
  return static_cast<CaptionController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CaptionController* CaptionControllerFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<CaptionController*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
CaptionControllerFactory* CaptionControllerFactory::GetInstance() {
  static base::NoDestructor<CaptionControllerFactory> factory;
  return factory.get();
}

CaptionControllerFactory::CaptionControllerFactory()
    : BrowserContextKeyedServiceFactory(
          "CaptionController",
          BrowserContextDependencyManager::GetInstance()) {}

CaptionControllerFactory::~CaptionControllerFactory() = default;

content::BrowserContext* CaptionControllerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* CaptionControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new CaptionController(
      Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace captions
