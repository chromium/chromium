// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption_controller_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/live_caption/live_caption_controller.h"

namespace captions {

// static
LiveCaptionController* LiveCaptionControllerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LiveCaptionController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LiveCaptionController* LiveCaptionControllerFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<LiveCaptionController*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
LiveCaptionControllerFactory* LiveCaptionControllerFactory::GetInstance() {
  static base::NoDestructor<LiveCaptionControllerFactory> factory;
  return factory.get();
}

LiveCaptionControllerFactory::LiveCaptionControllerFactory()
    : BrowserContextKeyedServiceFactory(
          "LiveCaptionController",
          BrowserContextDependencyManager::GetInstance()) {}

LiveCaptionControllerFactory::~LiveCaptionControllerFactory() = default;

content::BrowserContext* LiveCaptionControllerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* LiveCaptionControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LiveCaptionController(
      Profile::FromBrowserContext(context)->GetPrefs(),
      g_browser_process->local_state(), context);
}

}  // namespace captions
